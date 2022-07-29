
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
	UploadedData data;
	if ((uint32_t)Info.USize > renderer->Device->PhysicalDevice.Properties.limits.maxImageDimension2D || (uint32_t)Info.VSize > renderer->Device->PhysicalDevice.Properties.limits.maxImageDimension2D)
	{
		// To do: texture is too big. find the first mipmap level that fits and use that as the base size
		data = UploadWhite();
	}
	else
	{
		TextureUploader* uploader = TextureUploader::GetUploader(Info.Format);
		if (uploader)
			data = UploadData(Info, masked, uploader);
		else
			data = UploadWhite();
	}

	if (!tex->image)
	{
		tex->image = ImageBuilder()
			.Format(data.imageFormat)
			.Size(data.width, data.height, data.miplevels.size())
			.Usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
			.DebugName("CachedTexture.Image")
			.Create(renderer->Device);

		tex->imageView = ImageViewBuilder()
			.Image(tex->image.get(), data.imageFormat)
			.DebugName("CachedTexture.ImageView")
			.Create(renderer->Device);
	}

	auto cmdbuffer = renderer->Commands->GetTransferCommands();

	PipelineBarrier()
		.AddImage(
			tex->image.get(),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, data.miplevels.size())
		.Execute(
			cmdbuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

	cmdbuffer->copyBufferToImage(data.stagingbuffer->buffer, tex->image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, data.miplevels.size(), data.miplevels.data());

	PipelineBarrier()
		.AddImage(
			tex->image.get(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, data.miplevels.size())
		.Execute(
			cmdbuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	renderer->Commands->FrameDeleteList->buffers.push_back(std::move(data.stagingbuffer));
}

void UploadManager::UploadTextureRect(CachedTexture* tex, const FTextureInfo& Info, int xx, int yy, int w, int h)
{
	if (Info.Format != TEXF_RGBA7 || Info.NumMips < 1 || xx < 0 || yy < 0 || w <= 0 || h <= 0 || xx + w > Info.Mips[0]->USize || yy + h > Info.Mips[0]->VSize || !Info.Mips[0]->DataPtr)
		return;

	size_t pixelsSize = w * h * 4;
	pixelsSize = (pixelsSize + 15) / 16 * 16; // memory alignment

	auto stagingbuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.Size(pixelsSize)
		.DebugName("UpdateRectStaging")
		.Create(renderer->Device);

	FMipmapBase* Mip = Info.Mips[0];

	auto data = (BYTE*)stagingbuffer->Map(0, pixelsSize);

	auto Ptr = (FColor*)data;
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

	stagingbuffer->Unmap();

	auto cmdbuffer = renderer->Commands->GetTransferCommands();

	PipelineBarrier()
		.AddImage(
			tex->image.get(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, 1)
		.Execute(
			cmdbuffer,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { (int32_t)xx, (int32_t)yy, 0 };
	region.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };
	cmdbuffer->copyBufferToImage(stagingbuffer->buffer, tex->image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	PipelineBarrier()
		.AddImage(
			tex->image.get(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, 1)
		.Execute(
			cmdbuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	renderer->Commands->FrameDeleteList->buffers.push_back(std::move(stagingbuffer));
}

UploadManager::UploadedData UploadManager::UploadData(const FTextureInfo& Info, bool masked, TextureUploader* uploader)
{
	uploader->Begin(Info, masked);

	UploadedData result;
	result.imageFormat = uploader->GetVkFormat();
	result.width = Info.USize;
	result.height = Info.VSize;

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

	result.stagingbuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.Size(pixelsSize)
		.DebugName("CachedTexture.UploadDataStaging")
		.Create(renderer->Device);

	auto data = (BYTE*)result.stagingbuffer->Map(0, pixelsSize);
	auto Ptr = data;

	for (INT level = 0; level < Info.NumMips; level++)
	{
		FMipmapBase* Mip = Info.Mips[level];
		if (Mip->DataPtr)
		{
			uint32_t mipwidth = Mip->USize;
			uint32_t mipheight = Mip->VSize;

			VkBufferImageCopy region = {};
			region.bufferOffset = (VkDeviceSize)((uint8_t*)Ptr - (uint8_t*)data);
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = level;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = { mipwidth, mipheight, 1 };
			result.miplevels.push_back(region);

			INT mipsize = uploader->GetMipSize(Mip);
			uploader->UploadMip(Mip, Ptr);
			mipsize = (mipsize + 15) / 16 * 16; // memory alignment
			Ptr += mipsize;
		}
	}

	result.stagingbuffer->Unmap();
	uploader->End();
	return result;
}

UploadManager::UploadedData UploadManager::UploadWhite()
{
	UploadedData result;
	result.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	result.width = 1;
	result.height = 1;

	result.stagingbuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.Size(4)
		.DebugName("CachedTexture.UploadWhite")
		.Create(renderer->Device);

	auto data = (uint32_t*)result.stagingbuffer->Map(0, 4);
	data[0] = 0xffffffff;
	result.stagingbuffer->Unmap();

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = { 1, 1, 1 };
	result.miplevels.push_back(region);

	return result;
}
