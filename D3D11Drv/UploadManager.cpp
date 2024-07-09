
#include "Precomp.h"
#include "UploadManager.h"
#include "UD3D11RenderDevice.h"
#include "CachedTexture.h"

UploadManager::UploadManager(UD3D11RenderDevice* renderer) : renderer(renderer)
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

	if ((uint32_t)Info.USize > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
		(uint32_t)Info.VSize > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
		!uploader)
	{
		width = 1;
		height = 1;
		mipcount = 1;
		uploader = nullptr;
	}

	DXGI_FORMAT format = uploader ? uploader->GetDxgiFormat() : DXGI_FORMAT_R8G8B8A8_UNORM;

	// Base texture must use complete 4x4 compression blocks in Direct3D 11 or some drivers crash.
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

		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = mipcount;
		texDesc.ArraySize = 1;
		texDesc.Format = format;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		//texDesc.MiscFlags = tex->DummyMipmapCount > 0 ? D3D11_RESOURCE_MISC_RESOURCE_CLAMP : 0;
		HRESULT result = renderer->Device->CreateTexture2D(&texDesc, nullptr, tex->Texture.TypedInitPtr());
		ThrowIfFailed(result, "CreateTexture2D(GameTexture) failed");

		//if (tex->DummyMipmapCount > 0)
		//	renderer->Context->SetResourceMinLOD(tex->Texture, (float)tex->DummyMipmapCount);

		//D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		//viewDesc.Format = format;
		//viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		//viewDesc.Texture2D.MostDetailedMip = tex->DummyMipmapCount;
		//viewDesc.Texture2D.MipLevels = -1;
		result = renderer->Device->CreateShaderResourceView(tex->Texture, nullptr/*&viewDesc*/, tex->View.TypedInitPtr());
		ThrowIfFailed(result, "CreateShaderResourceView(GameTexture) failed");
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

	D3D11_BOX box = {};
	box.left = x;
	box.top = y;
	box.right = x + w;
	box.bottom = y + h;
	box.back = 1;
	renderer->Context->UpdateSubresource(tex->Texture, D3D11CalcSubresource(0, 0, Info.NumMips), &box, data, pitch, 0);

	renderer->Stats.RectUploads++;
}

void UploadManager::UploadData(ID3D11Texture2D* image, const FTextureInfo& Info, bool masked, TextureUploader* uploader, int dummyMipmapCount, INT minSize)
{
	for (INT level = 0; level < Info.NumMips; level++)
	{
		FMipmapBase* Mip = Info.Mips[level];
		if (Mip->DataPtr)
		{
			uint32_t mipwidth = Max(Mip->USize, minSize);
			uint32_t mipheight = Max(Mip->VSize, minSize);

			INT mipsize = uploader->GetUploadSize(0, 0, mipwidth, mipheight);
			mipsize = (mipsize + 15) / 16 * 16; // memory alignment

			auto data = (uint32_t*)GetUploadBuffer(mipsize);
			uploader->UploadRect(data, Mip, 0, 0, mipwidth, mipheight, Info.Palette, masked);

			UINT pitch = uploader->GetUploadSize(0, 0, mipwidth, 1);

			D3D11_BOX box = {};
			box.right = mipwidth;
			box.bottom = mipheight;
			box.back = 1;

			UINT subresource = D3D11CalcSubresource(level + dummyMipmapCount, 0, Info.NumMips + dummyMipmapCount);
			renderer->Context->UpdateSubresource(image, subresource, &box, data, pitch, 0);
		}
	}
}

void UploadManager::UploadWhite(ID3D11Texture2D* image)
{
	auto data = (uint32_t*)GetUploadBuffer(sizeof(uint32_t));
	data[0] = 0xffffffff;

	D3D11_BOX box = {};
	box.right = 1;
	box.bottom = 1;
	box.back = 1;
	renderer->Context->UpdateSubresource(image, D3D11CalcSubresource(0, 0, 1), &box, data, 4, 0);
}

uint8_t* UploadManager::GetUploadBuffer(size_t size)
{
	// Using uint32_t vector for dword alignment
	size = (size + 3) / 4;
	if (UploadBuffer.size() < size)
		UploadBuffer.resize(size);
	return (uint8_t*)UploadBuffer.data();
}
