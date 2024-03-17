
#include "Precomp.h"
#include "UploadManager.h"
#include "UD3D12RenderDevice.h"
#include "CachedTexture.h"

UploadManager::UploadManager(UD3D12RenderDevice* renderer) : renderer(renderer)
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

	if ((uint32_t)Info.USize > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
		(uint32_t)Info.VSize > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
		!uploader)
	{
		width = 1;
		height = 1;
		mipcount = 1;
		uploader = nullptr;
	}

	DXGI_FORMAT format = uploader ? uploader->GetDxgiFormat() : DXGI_FORMAT_R8G8B8A8_UNORM;

	// Base texture must use complete 4x4 compression blocks in Direct3D 12 or some drivers crash.
	// 
	// We fix this by creating 1 or 2 additional mipmap levels that are empty.

#if defined(OLDUNREAL469SDK)
	INT minSize = Info.Format == TEXF_BC1 || (Info.Format >= TEXF_BC2 && Info.Format <= TEXF_BC6H) ? 4 : 0;
#else
	INT minSize = Info.Format == TEXF_DXT1 ? 4 : 0;
#endif

	if (!tex->Texture)
	{
		if (width < minSize || height < minSize)
		{
			if (width == 1 || height == 1)
			{
				width *= 4;
				height *= 4;
				mipcount += 2;
				tex->DummyMipmapCount = 2;
			}
			else
			{
				width *= 2;
				height *= 2;
				mipcount += 1;
				tex->DummyMipmapCount = 1;
			}
		}

		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Format = format;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = mipcount;
		texDesc.SampleDesc.Count = 1;

		D3D12MA::ALLOCATION_DESC allocationDesc = {};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		HRESULT result = renderer->MemAllocator->CreateResource(&allocationDesc, &texDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, &tex->Allocation, tex->Texture.GetIID(), tex->Texture.InitPtr());
		ThrowIfFailed(result, "CreateResource(GameTexture) failed");
	}

	if (uploader)
		UploadData(tex->Texture, Info, masked, uploader, tex->DummyMipmapCount, minSize);
	else
		UploadWhite(tex->Texture);

	renderer->Stats.Uploads++;
}

void UploadManager::UploadTextureRect(CachedTexture* tex, const FTextureInfo& Info, int x, int y, int w, int h)
{
	TextureUploader* uploader = TextureUploader::GetUploader(Info.Format);
	if (!uploader || Info.NumMips < 1 || x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > Info.Mips[0]->USize || y + h > Info.Mips[0]->VSize || !Info.Mips[0]->DataPtr)
		return;

	size_t pixelsSize = uploader->GetUploadSize(x, y, w, h);
	pixelsSize = (pixelsSize + 15) / 16 * 16; // memory alignment

	uint8_t* data = GetUploadBuffer(pixelsSize);
	uploader->UploadRect(data, Info.Mips[0], x, y, w, h, Info.Palette, false);

	UINT pitch = uploader->GetUploadSize(0, 0, w, 1);

	auto onWriteSubresource = [&](uint8_t* dest, int subresource, const D3D12_SUBRESOURCE_FOOTPRINT& footprint)
		{
			const uint8_t* src = data;
			for (int y = 0; y < h; y++)
			{
				memcpy(dest, src, pitch);
				src += pitch;
				dest += footprint.RowPitch;
			}
		};

	renderer->UploadTexture(tex->Texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, x, y, w, h, 0, 1, onWriteSubresource);

	renderer->Stats.RectUploads++;
}

void UploadManager::UploadData(ID3D12Resource* image, const FTextureInfo& Info, bool masked, TextureUploader* uploader, int dummyMipmapCount, INT minSize)
{
	uint32_t width = Info.USize << dummyMipmapCount;
	uint32_t height = Info.VSize << dummyMipmapCount;

	auto onWriteSubresource = [&](uint8_t* dest, int subresource, const D3D12_SUBRESOURCE_FOOTPRINT& footprint)
		{
			int level = subresource - dummyMipmapCount;
			if (level < 0)
				return;

			FMipmapBase* Mip = Info.Mips[level];
			if (Mip->DataPtr)
			{
				uint32_t mipwidth = Max(Mip->USize, minSize);
				uint32_t mipheight = Max(Mip->VSize, minSize);

				INT mipsize = uploader->GetUploadSize(0, 0, mipwidth, mipheight);
				mipsize = (mipsize + 15) / 16 * 16; // memory alignment

				auto data = GetUploadBuffer(mipsize);
				uploader->UploadRect(data, Mip, 0, 0, mipwidth, mipheight, Info.Palette, masked);

				UINT pitch = uploader->GetUploadSize(0, 0, mipwidth, 1);

				uint32_t h = uploader->GetUploadHeight(mipheight);
				const uint8_t* src = data;
				for (uint32_t y = 0; y < h; y++)
				{
					memcpy(dest, src, pitch);
					src += pitch;
					dest += footprint.RowPitch;
				}
			}
		};

	renderer->UploadTexture(image, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, width, height, 0, Info.NumMips + dummyMipmapCount, onWriteSubresource);
}

void UploadManager::UploadWhite(ID3D12Resource* image)
{
	auto onWriteSubresource = [this](uint8_t* dest, int subresource, const D3D12_SUBRESOURCE_FOOTPRINT& footprint)
		{
			auto data = (uint32_t*)dest;
			data[0] = 0xffffffff;
		};

	renderer->UploadTexture(image, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 1, 1, 0, 1, onWriteSubresource);
}

uint8_t* UploadManager::GetUploadBuffer(size_t size)
{
	// Using uint32_t vector for dword alignment
	size = (size + 3) / 4;
	if (UploadBuffer.size() < size)
		UploadBuffer.resize(size);
	return (uint8_t*)UploadBuffer.data();
}
