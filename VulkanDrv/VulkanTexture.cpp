
#include "Precomp.h"
#include "VulkanTexture.h"
#include "VulkanObjects.h"
#include "VulkanBuilders.h"
#include "PixelBuffer.h"
#include "Renderer.h"

VulkanTexture::VulkanTexture(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags)
{
	Update(renderer, Info, PolyFlags);
}

VulkanTexture::~VulkanTexture()
{
}

void VulkanTexture::Update(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags)
{
	UMult = 1.0f / (Info.UScale * Info.USize);
	VMult = 1.0f / (Info.VScale * Info.VSize);

	UploadedData data;
	if ((uint32_t)Info.USize > renderer->Device->physicalDevice.properties.limits.maxImageDimension2D || (uint32_t)Info.VSize > renderer->Device->physicalDevice.properties.limits.maxImageDimension2D)
	{
		// To do: texture is too big. find the first mipmap level that fits and use that as the base size
		data = UploadWhite(renderer, Info, PolyFlags);
	}
	else
	{
		switch (Info.Format)
		{
		case TEXF_P8: data = UploadP8(renderer, Info, PolyFlags); break;
		case TEXF_RGBA7: data = UploadRGBA7(renderer, Info, PolyFlags); break;
		case TEXF_RGB16: data = UploadRGB16(renderer, Info, PolyFlags); break;
		case TEXF_DXT1: data = UploadDXT1(renderer, Info, PolyFlags); break;
		case TEXF_RGB8: data = UploadRGB8(renderer, Info, PolyFlags); break;
		case TEXF_RGBA8: data = UploadRGBA8(renderer, Info, PolyFlags); break;
		default: data = UploadWhite(renderer, Info, PolyFlags); break;
		}
	}

	if (!image)
	{
		ImageBuilder imgbuilder;
		imgbuilder.setFormat(data.imageFormat);
		imgbuilder.setSize(data.width, data.height, data.miplevels.size());
		imgbuilder.setUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		image = imgbuilder.create(renderer->Device);

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(image.get(), data.imageFormat);
		imageView = viewbuilder.create(renderer->Device);
	}

	auto cmdbuffer = renderer->GetTransferCommands();

	PipelineBarrier imageTransition0;
	imageTransition0.addImage(image.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, data.miplevels.size());
	imageTransition0.execute(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	cmdbuffer->copyBufferToImage(data.stagingbuffer->buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, data.miplevels.size(), data.miplevels.data());

	PipelineBarrier imageTransition1;
	imageTransition1.addImage(image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, data.miplevels.size());
	imageTransition1.execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	renderer->FrameDeleteList->buffers.push_back(std::move(data.stagingbuffer));
}

UploadedData VulkanTexture::UploadP8(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags)
{
	if (!Info.Palette)
		return UploadWhite(renderer, Info, PolyFlags);

	FColor NewPal[256];
	for (INT i = 0; i < 256; i++)
	{
		FColor& Src = Info.Palette[i];
		NewPal[i].R = Src.R; // ScaleR[Src.R];
		NewPal[i].G = Src.G; // ScaleG[Src.G];
		NewPal[i].B = Src.B; // ScaleB[Src.B];
		NewPal[i].A = Src.A;
	}
	if (PolyFlags & PF_Masked)
		NewPal[0] = FColor(0, 0, 0, 0);

	UploadedData result;
	result.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	result.width = Info.USize;
	result.height = Info.VSize;

	bool SkipMipmaps = Info.NumMips == 1;
	size_t pixelsSize = result.width * result.height * 4;
	if (Info.NumMips > 1)
		pixelsSize += (pixelsSize + 2) / 3;

	BufferBuilder builder;
	builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	builder.setSize(pixelsSize);
	result.stagingbuffer = builder.create(renderer->Device);

	auto data = (FColor*)result.stagingbuffer->Map(0, pixelsSize);

	FColor* Ptr = data;
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

			for (uint32_t y = 0; y < mipheight; y++)
			{
				BYTE* line = (BYTE*)Mip->DataPtr + y * Mip->USize;
				for (uint32_t x = 0; x < mipwidth; x++)
				{
					*Ptr++ = NewPal[line[x]];
				}
			}
		}
	}

	result.stagingbuffer->Unmap();
	return result;
}

UploadedData VulkanTexture::UploadRGBA7(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags)
{
	UploadedData result;
	result.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	result.width = Info.USize;
	result.height = Info.VSize;

	bool SkipMipmaps = Info.NumMips == 1;
	size_t pixelsSize = result.width * result.height * 4;
	if (Info.NumMips > 1)
		pixelsSize += (pixelsSize + 2) / 3;

	BufferBuilder builder;
	builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	builder.setSize(pixelsSize);
	result.stagingbuffer = builder.create(renderer->Device);

	auto data = (FColor*)result.stagingbuffer->Map(0, pixelsSize);

	FColor* Ptr = data;
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

			for (uint32_t y = 0; y < mipheight; y++)
			{
				FColor* line = (FColor*)Mip->DataPtr + y * Mip->USize;
				for (uint32_t x = 0; x < mipwidth; x++)
				{
					const FColor& Src = line[x];
					Ptr->R = Src.B; // ScaleR[Src.B];
					Ptr->G = Src.G; // ScaleG[Src.G];
					Ptr->B = Src.R; // ScaleB[Src.R];
					Ptr->A = Src.A * 2;
					Ptr++;
				}
			}
		}
	}

	result.stagingbuffer->Unmap();
	return result;
}

UploadedData VulkanTexture::UploadRGB16(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags)
{
	UploadedData result;
	result.imageFormat = VK_FORMAT_R5G6B5_UNORM_PACK16;
	result.width = Info.USize;
	result.height = Info.VSize;

	bool SkipMipmaps = Info.NumMips == 1;
	size_t pixelsSize = result.width * result.height * 3;
	if (Info.NumMips > 1)
		pixelsSize += (pixelsSize + 2) / 3;

	BufferBuilder builder;
	builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	builder.setSize(pixelsSize);
	result.stagingbuffer = builder.create(renderer->Device);

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

			INT mipsize = Mip->USize * Mip->VSize * 2;
			memcpy(Ptr, Mip->DataPtr, mipsize);
			Ptr += mipsize;
		}
	}

	result.stagingbuffer->Unmap();
	return result;
}

UploadedData VulkanTexture::UploadDXT1(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags)
{
	UploadedData result;
	result.imageFormat = VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	result.width = Info.USize;
	result.height = Info.VSize;

	bool SkipMipmaps = Info.NumMips == 1;
	size_t pixelsSize = (result.width * result.height + result.width + 1) / 2;
	if (Info.NumMips > 1)
		pixelsSize += (pixelsSize + 2) / 3;

	BufferBuilder builder;
	builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	builder.setSize(pixelsSize);
	result.stagingbuffer = builder.create(renderer->Device);

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

			INT mipsize = Mip->USize * Mip->VSize / 2;
			memcpy(Ptr, Mip->DataPtr, mipsize);
			Ptr += mipsize;
		}
	}

	result.stagingbuffer->Unmap();
	return result;
}

UploadedData VulkanTexture::UploadRGB8(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags)
{
	UploadedData result;
	result.imageFormat = VK_FORMAT_R8G8B8_UNORM;
	result.width = Info.USize;
	result.height = Info.VSize;

	bool SkipMipmaps = Info.NumMips == 1;
	size_t pixelsSize = result.width * result.height * 3;
	if (Info.NumMips > 1)
		pixelsSize += (pixelsSize + 2) / 3;

	BufferBuilder builder;
	builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	builder.setSize(pixelsSize);
	result.stagingbuffer = builder.create(renderer->Device);

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

			INT mipsize = Mip->USize * Mip->VSize * 3;
			memcpy(Ptr, Mip->DataPtr, mipsize);
			Ptr += mipsize;
		}
	}

	result.stagingbuffer->Unmap();
	return result;
}

UploadedData VulkanTexture::UploadRGBA8(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags)
{
	UploadedData result;
	result.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	result.width = Info.USize;
	result.height = Info.VSize;

	bool SkipMipmaps = Info.NumMips == 1;
	size_t pixelsSize = result.width * result.height * 4;
	if (Info.NumMips > 1)
		pixelsSize += (pixelsSize + 2) / 3;

	BufferBuilder builder;
	builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	builder.setSize(pixelsSize);
	result.stagingbuffer = builder.create(renderer->Device);

	auto data = (FColor*)result.stagingbuffer->Map(0, pixelsSize);
	FColor* Ptr = data;

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

			INT mipsize = Mip->USize * Mip->VSize;
			memcpy(Ptr, Mip->DataPtr, mipsize * 4);
			Ptr += mipsize;
		}
	}

	result.stagingbuffer->Unmap();
	return result;
}

UploadedData VulkanTexture::UploadWhite(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags)
{
	UploadedData result;
	result.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	result.width = 1;
	result.height = 1;

	BufferBuilder builder;
	builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	builder.setSize(4);
	result.stagingbuffer = builder.create(renderer->Device);
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
