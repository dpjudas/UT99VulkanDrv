
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

void VulkanTexture::Update(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags)
{
	INT MaxLogUOverV = 12;
	INT MaxLogVOverU = 12;
	INT MinLogTextureSize = 0;
	INT MaxLogTextureSize = 12;

	INT BaseMip = Min(0, Info.NumMips - 1);
	INT UCopyBits = 0;
	INT VCopyBits = 0;
	INT UBits = Info.Mips[BaseMip]->UBits;
	INT VBits = Info.Mips[BaseMip]->VBits;

	if (UBits - VBits > MaxLogUOverV)
	{
		VCopyBits += (UBits - VBits) - MaxLogUOverV;
		VBits = UBits - MaxLogUOverV;
	}
	if (VBits - UBits > MaxLogVOverU)
	{
		UCopyBits += (VBits - UBits) - MaxLogVOverU;
		UBits = VBits - MaxLogVOverU;
	}
	if (UBits < MinLogTextureSize)
	{
		UCopyBits += MinLogTextureSize - UBits;
		UBits += MinLogTextureSize - UBits;
	}
	if (VBits < MinLogTextureSize)
	{
		VCopyBits += MinLogTextureSize - VBits;
		VBits += MinLogTextureSize - VBits;
	}
	if (UBits > MaxLogTextureSize)
	{
		BaseMip += UBits - MaxLogTextureSize;
		VBits -= UBits - MaxLogTextureSize;
		UBits = MaxLogTextureSize;
		if (VBits < 0)
		{
			VCopyBits = -VBits;
			VBits = 0;
		}
	}
	if (VBits > MaxLogTextureSize)
	{
		BaseMip += VBits - MaxLogTextureSize;
		UBits -= VBits - MaxLogTextureSize;
		VBits = MaxLogTextureSize;
		if (UBits < 0)
		{
			UCopyBits = -UBits;
			UBits = 0;
		}
	}

	UMult = 1.0f / (Info.UScale * (Info.USize << UCopyBits));
	VMult = 1.0f / (Info.VScale * (Info.VSize << VCopyBits));

	//BYTE* ScaleR = &ScaleByte[PYR(Info.MaxColor->R)];
	//BYTE* ScaleG = &ScaleByte[PYR(Info.MaxColor->G)];
	//BYTE* ScaleB = &ScaleByte[PYR(Info.MaxColor->B)];

	FColor NewPal[256];
	if (Info.Palette)
	{
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
	}

	bool SkipMipmaps = Info.NumMips == 1;

	size_t pixelsSize = (1 << (UBits + VBits)) * 4;
	if (!SkipMipmaps)
		pixelsSize += (pixelsSize + 2) / 3;

	BufferBuilder builder;
	builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	builder.setSize(pixelsSize);
	auto stagingbuffer = builder.create(renderer->Device);
	auto data = (FColor*)stagingbuffer->Map(0, pixelsSize);

	std::vector<VkBufferImageCopy> miplevels;
	FColor* Ptr = data;
	for (INT Level = 0; Level <= Max(UBits, VBits); Level++)
	{
		INT MipIndex = BaseMip + Level;
		INT StepBits = 0;
		if (MipIndex >= Info.NumMips)
		{
			StepBits = MipIndex - (Info.NumMips - 1);
			MipIndex = Info.NumMips - 1;
		}

		FMipmapBase* Mip = Info.Mips[MipIndex];
		DWORD Mask = Mip->USize - 1;
		if (Mip->DataPtr)
		{
			uint32_t mipwidth = (1 << Max(0, UBits - Level));
			uint32_t mipheight = (1 << Max(0, VBits - Level));

			VkBufferImageCopy region = {};
			region.bufferOffset = (VkDeviceSize)((uint8_t*)Ptr - (uint8_t*)data);
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = Level;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = { mipwidth, mipheight, 1 };
			miplevels.push_back(region);

			if (Info.Palette)
			{
				for (uint32_t i = 0; i < mipheight; i++)
				{
					BYTE* Base = (BYTE*)Mip->DataPtr + ((i << StepBits) & (Mip->VSize - 1)) * Mip->USize;
					for (INT j = 0; j < (1 << Max(0, UBits - Level + StepBits)); j += (1 << StepBits))
					{
						*Ptr++ = NewPal[Base[j & Mask]];
					}
				}
			}
			else
			{
				for (uint32_t i = 0; i < mipheight; i++)
				{
					FColor* Base = (FColor*)Mip->DataPtr + Min<DWORD>((i << StepBits) & (Mip->VSize - 1), Info.VClamp - 1) * Mip->USize;
					for (INT j = 0; j < (1 << Max(0, UBits - Level + StepBits)); j += (1 << StepBits))
					{
						FColor& Src = Base[Min<DWORD>(j & Mask, Info.UClamp - 1)];
						Ptr->R = Src.B; // ScaleR[Src.B];
						Ptr->G = Src.G; // ScaleG[Src.G];
						Ptr->B = Src.R; // ScaleB[Src.R];
						Ptr->A = Src.A * 2;
						Ptr++;
					}
				}
			}
		}

		if (SkipMipmaps)
			break;
	}

	stagingbuffer->Unmap();

	if (!image)
	{
		VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

		ImageBuilder imgbuilder;
		imgbuilder.setFormat(imageFormat);
		imgbuilder.setSize(1 << UBits, 1 << VBits, miplevels.size());
		imgbuilder.setUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		image = imgbuilder.create(renderer->Device);

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(image.get(), imageFormat);
		imageView = viewbuilder.create(renderer->Device);
	}

	auto cmdbuffer = renderer->GetTransferCommands();

	PipelineBarrier imageTransition0;
	imageTransition0.addImage(image.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, miplevels.size());
	imageTransition0.execute(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	cmdbuffer->copyBufferToImage(stagingbuffer->buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, miplevels.size(), miplevels.data());

	PipelineBarrier imageTransition1;
	imageTransition1.addImage(image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, miplevels.size());
	imageTransition1.execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	renderer->FrameDeleteList->buffers.push_back(std::move(stagingbuffer));
}

VulkanTexture::~VulkanTexture()
{
}
