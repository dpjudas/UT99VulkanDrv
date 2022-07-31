
#include "Precomp.h"
#include "TextureUploader.h"

TextureUploader* TextureUploader::GetUploader(ETextureFormat format)
{
	static std::map<ETextureFormat, std::unique_ptr<TextureUploader>> Uploaders;
	if (Uploaders.empty())
	{
		// Original.
		Uploaders[TEXF_P8].reset(new TextureUploader_P8());
		Uploaders[TEXF_BGRA8_LM].reset(new TextureUploader_BGRA8_LM());
		Uploaders[TEXF_R5G6B5].reset(new TextureUploader_Simple(VK_FORMAT_R5G6B5_UNORM_PACK16, 2));
		Uploaders[TEXF_BC1].reset(new TextureUploader_4x4Block(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8));
		Uploaders[TEXF_RGB8].reset(new TextureUploader_Simple(VK_FORMAT_R8G8B8_UNORM, 3));
		Uploaders[TEXF_BGRA8].reset(new TextureUploader_Simple(VK_FORMAT_B8G8R8A8_UNORM, 4));

		// S3TC (continued).
		Uploaders[TEXF_BC2].reset(new TextureUploader_4x4Block(VK_FORMAT_BC2_UNORM_BLOCK, 16));
		Uploaders[TEXF_BC3].reset(new TextureUploader_4x4Block(VK_FORMAT_BC3_UNORM_BLOCK, 16));

		// RGTC.
		Uploaders[TEXF_BC4].reset(new TextureUploader_4x4Block(VK_FORMAT_BC4_UNORM_BLOCK, 8));
		Uploaders[TEXF_BC4_S].reset(new TextureUploader_4x4Block(VK_FORMAT_BC4_SNORM_BLOCK, 8));
		Uploaders[TEXF_BC5].reset(new TextureUploader_4x4Block(VK_FORMAT_BC5_UNORM_BLOCK, 16));
		Uploaders[TEXF_BC5_S].reset(new TextureUploader_4x4Block(VK_FORMAT_BC5_SNORM_BLOCK, 16));

		// BPTC.
		Uploaders[TEXF_BC7].reset(new TextureUploader_4x4Block(VK_FORMAT_BC7_UNORM_BLOCK, 16));
		Uploaders[TEXF_BC6H_S].reset(new TextureUploader_4x4Block(VK_FORMAT_BC6H_SFLOAT_BLOCK, 16));
		Uploaders[TEXF_BC6H].reset(new TextureUploader_4x4Block(VK_FORMAT_BC6H_UFLOAT_BLOCK, 16));

		// Normalized RGBA.
		Uploaders[TEXF_RGBA16].reset(new TextureUploader_Simple(VK_FORMAT_R16G16B16A16_UNORM, 8));
		Uploaders[TEXF_RGBA16_S].reset(new TextureUploader_Simple(VK_FORMAT_R16G16B16A16_SNORM, 8));
		//Uploaders[TEXF_RGBA32].reset(new TextureUploader_Simple(VK_FORMAT_R32G32B32A32_UNORM, 16));
		//Uploaders[TEXF_RGBA32_S].reset(new TextureUploader_Simple(VK_FORMAT_R32G32B32A32_SNORM, 16));

		// S3TC (continued).
		Uploaders[TEXF_BC1_PA].reset(new TextureUploader_4x4Block(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8));

		// Normalized RGBA (continued).
		Uploaders[TEXF_R8].reset(new TextureUploader_Simple(VK_FORMAT_R8_UNORM, 1));
		Uploaders[TEXF_R8_S].reset(new TextureUploader_Simple(VK_FORMAT_R8_SNORM, 1));
		Uploaders[TEXF_R16].reset(new TextureUploader_Simple(VK_FORMAT_R16_UNORM, 2));
		Uploaders[TEXF_R16_S].reset(new TextureUploader_Simple(VK_FORMAT_R16_SNORM, 2));
		//Uploaders[TEXF_R32].reset(new TextureUploader_Simple(VK_FORMAT_R32_UNORM, 4));
		//Uploaders[TEXF_R32_S].reset(new TextureUploader_Simple(VK_FORMAT_R32_SNORM, 4));
		Uploaders[TEXF_RG8].reset(new TextureUploader_Simple(VK_FORMAT_R8G8_UNORM, 2));
		Uploaders[TEXF_RG8_S].reset(new TextureUploader_Simple(VK_FORMAT_R8G8_SNORM, 2));
		Uploaders[TEXF_RG16].reset(new TextureUploader_Simple(VK_FORMAT_R16G16_UNORM, 4));
		Uploaders[TEXF_RG16_S].reset(new TextureUploader_Simple(VK_FORMAT_R16G16_SNORM, 4));
		//Uploaders[TEXF_RG32].reset(new TextureUploader_Simple(VK_FORMAT_R32G32_UNORM, 8));
		//Uploaders[TEXF_RG32_S].reset(new TextureUploader_Simple(VK_FORMAT_R32G32_SNORM, 8));
		Uploaders[TEXF_RGB8_S].reset(new TextureUploader_Simple(VK_FORMAT_R8G8B8_SNORM, 3));
		Uploaders[TEXF_RGB16_].reset(new TextureUploader_Simple(VK_FORMAT_R16G16B16_UNORM, 6));
		Uploaders[TEXF_RGB16_S].reset(new TextureUploader_Simple(VK_FORMAT_R16G16B16_SNORM, 6));
		//Uploaders[TEXF_RGB32].reset(new TextureUploader_Simple(VK_FORMAT_R32G32B32_UNORM, 12));
		//Uploaders[TEXF_RGB32_S].reset(new TextureUploader_Simple(VK_FORMAT_R32G32B32_SNORM, 12));
		Uploaders[TEXF_RGBA8_].reset(new TextureUploader_Simple(VK_FORMAT_R8G8B8A8_UNORM, 4));
		Uploaders[TEXF_RGBA8_S].reset(new TextureUploader_Simple(VK_FORMAT_R8G8B8A8_SNORM, 4));

		// Floating point RGBA.
		Uploaders[TEXF_R16_F].reset(new TextureUploader_Simple(VK_FORMAT_R16_SFLOAT, 2));
		Uploaders[TEXF_R32_F].reset(new TextureUploader_Simple(VK_FORMAT_R32_SFLOAT, 4));
		Uploaders[TEXF_RG16_F].reset(new TextureUploader_Simple(VK_FORMAT_R16G16_SFLOAT, 4));
		Uploaders[TEXF_RG32_F].reset(new TextureUploader_Simple(VK_FORMAT_R32G32_SFLOAT, 8));
		Uploaders[TEXF_RGB16_F].reset(new TextureUploader_Simple(VK_FORMAT_R16G16B16_SFLOAT, 6));
		Uploaders[TEXF_RGB32_F].reset(new TextureUploader_Simple(VK_FORMAT_R32G32B32_SFLOAT, 12));
		Uploaders[TEXF_RGBA16_F].reset(new TextureUploader_Simple(VK_FORMAT_R16G16B16A16_SFLOAT, 8));
		Uploaders[TEXF_RGBA32_F].reset(new TextureUploader_Simple(VK_FORMAT_R32G32B32A32_SFLOAT, 16));

		// Special.
		//Uploaders[TEXF_ARGB8].reset(new TextureUploader_ARGB8(VK_FORMAT_R16_SFLOAT, 2));
		//Uploaders[TEXF_RGB10A2].reset(new TextureUploader_RGB10A2(VK_FORMAT_R16_SFLOAT, 2));
		//Uploaders[TEXF_RGB10A2_UI].reset(new TextureUploader_RGB10A2_UI(VK_FORMAT_R16_SFLOAT, 2));
		//Uploaders[TEXF_RGB10A2_LM].reset(new TextureUploader_RGB10A2_LM(VK_FORMAT_R16_SFLOAT, 2));
		//Uploaders[TEXF_RGB9E5].reset(new TextureUploader_RGB9E5(VK_FORMAT_R16_SFLOAT, 2));
		//Uploaders[TEXF_P8_RGB9E5].reset(new TextureUploader_P8_RGB9E5(VK_FORMAT_R16_SFLOAT, 2));
		//Uploaders[TEXF_R1].reset(new TextureUploader_R1(VK_FORMAT_R16_SFLOAT, 2));
		//Uploaders[TEXF_RGB10A2_S].reset(new TextureUploader_RGB10A2_S(VK_FORMAT_R16_SFLOAT, 2));
		//Uploaders[TEXF_RGB10A2_I].reset(new TextureUploader_RGB10A2_I(VK_FORMAT_R16_SFLOAT, 2));
		//Uploaders[TEXF_R11G11B10_F].reset(new TextureUploader_R11G11B10_F(VK_FORMAT_R16_SFLOAT, 2));
	}

	auto it = Uploaders.find(format);
	if (it != Uploaders.end())
		return it->second.get();
	else
		return nullptr;
}

/////////////////////////////////////////////////////////////////////////////

int TextureUploader_P8::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 4;
}

void TextureUploader_P8::UploadRect(void* d, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked)
{
	FColor black(0, 0, 0, 0);
	int pitch = mip->USize;
	BYTE* src = mip->DataPtr + x + y * pitch;
	FColor* Ptr = (FColor*)d;
	for (int i = 0; i < h; i++)
	{
		for (int x = 0; x < w; x++)
		{
			int idx = src[x];
			FColor c = (!masked || idx != 0) ? palette[idx] : black;
			Ptr->R = c.R;
			Ptr->G = c.G;
			Ptr->B = c.B;
			Ptr->A = c.A;
			Ptr++;
		}
		src += pitch;
	}
}

/////////////////////////////////////////////////////////////////////////////

int TextureUploader_BGRA8_LM::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 4;
}

void TextureUploader_BGRA8_LM::UploadRect(void* dst, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked)
{
	int pitch = mip->USize;
	FColor* src = ((FColor*)mip->DataPtr) + x + y * pitch;
	auto Ptr = (FColor*)dst;
	for (int i = 0; i < h; i++)
	{
		for (int x = 0; x < w; x++)
		{
			FColor Src = src[x];
			Ptr->R = Src.B << 1;
			Ptr->G = Src.G << 1;
			Ptr->B = Src.R << 1;
			Ptr->A = Src.A << 1;
			Ptr++;
		}
		src += pitch;
	}
}

/////////////////////////////////////////////////////////////////////////////

int TextureUploader_Simple::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * BytesPerPixel;
}

void TextureUploader_Simple::UploadRect(void* d, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked)
{
	int pitch = mip->USize * BytesPerPixel;
	int size = w * BytesPerPixel;
	BYTE* src = mip->DataPtr + x * BytesPerPixel + y * pitch;
	BYTE* dst = (BYTE*)d;
	for (int i = 0; i < h; i++)
	{
		memcpy(dst, src, size);
		dst += size;
		src += pitch;
	}
}

/////////////////////////////////////////////////////////////////////////////

int TextureUploader_4x4Block::GetUploadSize(int x, int y, int w, int h)
{
	int x0 = x / 4;
	int y0 = y / 4;
	int x1 = (x + w + 3) / 4;
	int y1 = (y + h + 3) / 4;
	return (x1 - x0) * (y0 - y1) * BytesPerBlock;
}

void TextureUploader_4x4Block::UploadRect(void* d, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked)
{
	int x0 = x / 4;
	int y0 = y / 4;
	int x1 = (x + w + 3) / 4;
	int y1 = (y + h + 3) / 4;

	int pitch = (mip->USize + 3) / 4 * BytesPerBlock;
	int size = (x1 - x0) * BytesPerBlock;
	BYTE* src = mip->DataPtr + x0 * BytesPerBlock + y0 * pitch;
	BYTE* dst = (BYTE*)d;
	for (int i = y0; i < y1; i++)
	{
		memcpy(dst, src, size);
		dst += size;
		src += pitch;
	}
}
