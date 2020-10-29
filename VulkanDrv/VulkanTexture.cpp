
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
		case TEXF_P8:
			{
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

				data = UploadData(renderer, Info, PolyFlags, VK_FORMAT_R8G8B8A8_UNORM, [](auto mip) { return mip->USize * mip->VSize * 4; },
					[&](auto mip, auto dst)
					{
						auto Ptr = (FColor*)dst;
						uint32_t mipwidth = mip->USize;
						uint32_t mipheight = mip->VSize;
						for (uint32_t y = 0; y < mipheight; y++)
						{
							BYTE* line = (BYTE*)mip->DataPtr + y * mip->USize;
							for (uint32_t x = 0; x < mipwidth; x++)
							{
								*Ptr++ = NewPal[line[x]];
							}
						}
					});
			}
			break;
		case TEXF_RGBA7:
			data = UploadData(renderer, Info, PolyFlags, VK_FORMAT_R8G8B8A8_UNORM, [](auto mip) { return mip->USize * mip->VSize * 4; },
				[](auto mip, auto dst)
				{
					auto Ptr = (FColor*)dst;
					uint32_t mipwidth = mip->USize;
					uint32_t mipheight = mip->VSize;
					for (uint32_t y = 0; y < mipheight; y++)
					{
						FColor* line = (FColor*)mip->DataPtr + y * mip->USize;
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
				});
			break;
		case TEXF_RGB16: data = UploadData(renderer, Info, PolyFlags, VK_FORMAT_R5G6B5_UNORM_PACK16, [](auto mip) { return mip->USize * mip->VSize * 2; }); break;
		case TEXF_DXT1: data = UploadData(renderer, Info, PolyFlags, VK_FORMAT_BC1_RGB_UNORM_BLOCK, [](auto mip) { return mip->USize * mip->VSize / 2; }); break;
		case TEXF_RGB8: data = UploadData(renderer, Info, PolyFlags, VK_FORMAT_R8G8B8_UNORM, [](auto mip) { return mip->USize * mip->VSize * 3; }); break;
		case TEXF_RGBA8: data = UploadData(renderer, Info, PolyFlags, VK_FORMAT_B8G8R8A8_UNORM, [](auto mip) { return mip->USize * mip->VSize * 4; }); break;
		case 0x06/*TEXF_BC2*/: data = UploadData(renderer, Info, PolyFlags, VK_FORMAT_BC2_UNORM_BLOCK, [](auto mip) { return mip->USize * mip->VSize; }); break;
		case 0x07/*TEXF_BC3*/: data = UploadData(renderer, Info, PolyFlags, VK_FORMAT_BC3_UNORM_BLOCK, [](auto mip) { return mip->USize * mip->VSize; }); break;
		case 0x1a/*TEXF_BC1_PA*/: data = UploadData(renderer, Info, PolyFlags, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, [](auto mip) { return mip->USize * mip->VSize / 2; }); break;
		case 0x0c/*TEXF_BC7*/: data = UploadData(renderer, Info, PolyFlags, VK_FORMAT_BC7_UNORM_BLOCK, [](auto mip) { return mip->USize * mip->VSize; }); break;
		//case 0x0d/*TEXF_BC6H_S*/: data = UploadData(renderer, Info, PolyFlags, VK_FORMAT_BC6H_SFLOAT_BLOCK, [](auto mip) { return mip->USize * mip->VSize * ??; }); break;
		//case 0x0e/*TEXF_BC6H*/: data = UploadData(renderer, Info, PolyFlags, VK_FORMAT_BC6H_UFLOAT_BLOCK, [](auto mip) { return mip->USize * mip->VSize * ??; }); break;
		default: data = UploadWhite(renderer, Info, PolyFlags); break;
		}

		// To do: upgrade to the 469 SDK and implement all the new texture formats (since they are all essentially OpenGL/Vulkan standard formats it is trivial to do)
		// Important: the TEXF_RGBA8 is renamed to TEXF_BGRA8 in that version of the SDK!
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

UploadedData VulkanTexture::UploadData(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags, VkFormat imageFormat, std::function<int(FMipmapBase* mip)> calcMipSize, std::function<void(FMipmapBase* mip, void* dst)> copyMip)
{
	UploadedData result;
	result.imageFormat = imageFormat;
	result.width = Info.USize;
	result.height = Info.VSize;

	size_t pixelsSize = 0;
	for (INT level = 0; level < Info.NumMips; level++)
	{
		FMipmapBase* Mip = Info.Mips[level];
		if (Mip->DataPtr)
		{
			pixelsSize += calcMipSize(Mip);
		}
	}

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

			INT mipsize = calcMipSize(Mip);
			if (copyMip)
				copyMip(Mip, Ptr);
			else
				memcpy(Ptr, Mip->DataPtr, mipsize);
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
