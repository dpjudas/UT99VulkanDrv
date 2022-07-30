
#include "Precomp.h"
#include "UploadManager.h"
#include "UVulkanRenderDevice.h"
#include "CachedTexture.h"
#include "VulkanBuilders.h"

UploadManager::UploadManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
}

UploadManager::~UploadManager()
{
}

bool UploadManager::SupportsTextureFormat(ETextureFormat Format) const
{
	return TextureUploader::GetUploader(Format);
}

void UploadManager::UploadTexture(CachedTexture* tex, const FTextureInfo& Info, bool masked)
{
	int width = Info.USize;
	int height = Info.VSize;
	int mipcount = Info.NumMips;

	TextureUploader* uploader = TextureUploader::GetUploader(Info.Format);

	if ((uint32_t)Info.USize > renderer->Device->PhysicalDevice.Properties.limits.maxImageDimension2D ||
		(uint32_t)Info.VSize > renderer->Device->PhysicalDevice.Properties.limits.maxImageDimension2D ||
		!uploader)
	{
		width = 1;
		height = 1;
		mipcount = 1;
		uploader = nullptr;
	}

	VkFormat format = uploader ? uploader->GetVkFormat() : VK_FORMAT_R8G8B8A8_UNORM;

	if (!tex->image)
	{
		tex->image = ImageBuilder()
			.Format(format)
			.Size(width, height, mipcount)
			.Usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
			.DebugName("CachedTexture.Image")
			.Create(renderer->Device);

		tex->imageView = ImageViewBuilder()
			.Image(tex->image.get(), format)
			.DebugName("CachedTexture.ImageView")
			.Create(renderer->Device);
	}

	if (uploader)
		UploadData(tex->image->image, Info, masked, uploader);
	else
		UploadWhite(tex->image->image);
}

void UploadManager::UploadTextureRect(CachedTexture* tex, const FTextureInfo& Info, int xx, int yy, int w, int h)
{
	if (Info.Format != TEXF_RGBA7 || Info.NumMips < 1 || xx < 0 || yy < 0 || w <= 0 || h <= 0 || xx + w > Info.Mips[0]->USize || yy + h > Info.Mips[0]->VSize || !Info.Mips[0]->DataPtr)
		return;

	size_t pixelsSize = w * h * 4;
	pixelsSize = (pixelsSize + 15) / 16 * 16; // memory alignment

	WaitIfUploadBufferIsFull(pixelsSize);

	FMipmapBase* Mip = Info.Mips[0];

	auto Ptr = (FColor*)(renderer->Buffers->UploadData + UploadBufferPos);
	for (int y = yy; y < yy + h; y++)
	{
		FColor* line = (FColor*)Mip->DataPtr + y * Mip->USize;
		for (int x = xx; x < xx + w; x++)
		{
			const FColor& Src = line[x];
			Ptr->R = Src.B * 2;
			Ptr->G = Src.G * 2;
			Ptr->B = Src.R * 2;
			Ptr->A = Src.A * 2;
			Ptr++;
		}
	}

	VkBufferImageCopy region = {};
	region.bufferOffset = UploadBufferPos;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { (int32_t)xx, (int32_t)yy, 0 };
	region.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };
	RectUploads[tex->image->image].push_back(region);

	UploadBufferPos += pixelsSize;
}

void UploadManager::UploadData(VkImage image, const FTextureInfo& Info, bool masked, TextureUploader* uploader)
{
	uploader->Begin(Info, masked);

	size_t pixelsSize = 0;
	for (INT level = 0; level < Info.NumMips; level++)
	{
		FMipmapBase* Mip = Info.Mips[level];
		if (Mip->DataPtr)
		{
			INT mipsize = uploader->GetMipSize(Mip);
			mipsize = (mipsize + 15) / 16 * 16; // memory alignment
			pixelsSize += mipsize;
		}
	}

	WaitIfUploadBufferIsFull(pixelsSize);

	UploadedTexture upload;
	upload.Image = image;
	upload.Index = (int)ImageCopies.size();
	upload.Count = Info.NumMips;
	Uploads.push_back(upload);

	uint8_t* data = renderer->Buffers->UploadData;
	uint8_t* Ptr = data + UploadBufferPos;

	for (INT level = 0; level < Info.NumMips; level++)
	{
		FMipmapBase* Mip = Info.Mips[level];
		if (Mip->DataPtr)
		{
			uint32_t mipwidth = Mip->USize;
			uint32_t mipheight = Mip->VSize;

			VkBufferImageCopy region = {};
			region.bufferOffset = (VkDeviceSize)(Ptr - data);
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = level;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = { mipwidth, mipheight, 1 };
			ImageCopies.push_back(region);

			INT mipsize = uploader->GetMipSize(Mip);
			uploader->UploadMip(Mip, Ptr);
			mipsize = (mipsize + 15) / 16 * 16; // memory alignment
			Ptr += mipsize;
		}
	}

	UploadBufferPos += pixelsSize;

	uploader->End();
}

void UploadManager::UploadWhite(VkImage image)
{
	WaitIfUploadBufferIsFull(16);

	UploadedTexture upload;
	upload.Image = image;
	upload.Index = (int)ImageCopies.size();
	upload.Count = 1;

	auto data = (uint32_t*)(renderer->Buffers->UploadData + UploadBufferPos);
	data[0] = 0xffffffff;

	VkBufferImageCopy region = {};
	region.bufferOffset = UploadBufferPos;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = { 1, 1, 1 };

	Uploads.push_back(upload);
	ImageCopies.push_back(region);
	UploadBufferPos += 16; // 16-byte aligned
}

void UploadManager::WaitIfUploadBufferIsFull(int bytes)
{
	if (UploadBufferPos + bytes > BufferManager::UploadBufferSize)
	{
		renderer->Commands->WaitForTransfer();
	}
}

void UploadManager::SubmitUploads()
{
	if (Uploads.empty() && RectUploads.empty())
		return;

	// Group all rect uploads for each texture into one combined vkCopyBufferToImage call
	bool partialUpdates = !RectUploads.empty();
	for (auto& it : RectUploads)
	{
		UploadedTexture upload;
		upload.Image = it.first;
		upload.Index = (int)ImageCopies.size();
		upload.Count = (int)it.second.size();
		upload.PartialUpdate = true;
		Uploads.push_back(upload);
		ImageCopies.insert(ImageCopies.end(), it.second.begin(), it.second.end());
		renderer->Stats.RectUploads += it.second.size();
	}
	RectUploads.clear();

	renderer->Stats.Uploads += Uploads.size();

	auto cmdbuffer = renderer->Commands->GetTransferCommands();

	// Transition images for upload
	PipelineBarrier beforeBarrier;
	for (const UploadedTexture& upload : Uploads)
	{
		beforeBarrier.AddImage(
			upload.Image,
			upload.PartialUpdate ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			upload.PartialUpdate ? VK_ACCESS_SHADER_READ_BIT : 0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, upload.Count);
	}
	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	if (partialUpdates)
		srcStage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	beforeBarrier.Execute(cmdbuffer, srcStage , VK_PIPELINE_STAGE_TRANSFER_BIT);

	// Copy from buffer to images
	VkBuffer buffer = renderer->Buffers->UploadBuffer->buffer;
	for (const UploadedTexture& upload : Uploads)
	{
		cmdbuffer->copyBufferToImage(buffer, upload.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, upload.Count, ImageCopies.data() + upload.Index);
	}

	// Transition images for texture sampling
	PipelineBarrier afterBarrier;
	for (const UploadedTexture& upload : Uploads)
	{
		afterBarrier.AddImage(
			upload.Image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, upload.Count);
	}
	afterBarrier.Execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	UploadBufferPos = 0;
	Uploads.clear();
	ImageCopies.clear();
}
