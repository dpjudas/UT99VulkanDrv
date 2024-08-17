
#include "Precomp.h"
#include "TextureManager.h"
#include "UD3D11RenderDevice.h"
#include "CachedTexture.h"

TextureManager::TextureManager(UD3D11RenderDevice* renderer) : renderer(renderer)
{
}

TextureManager::~TextureManager()
{
	ClearCache();
}

void TextureManager::UpdateTextureRect(FTextureInfo* info, int x, int y, int w, int h)
{
	std::unique_ptr<CachedTexture>& tex = TextureCache[0][info->CacheID];
	if (tex)
	{
		renderer->Uploads->UploadTextureRect(tex.get(), *info, x, y, w, h);
		info->bRealtimeChanged = 0;
	}
}

CachedTexture* TextureManager::GetTexture(FTextureInfo* info, bool masked)
{
	if (!info)
		return GetNullTexture();

	if (info->Format != TEXF_P8)
		masked = false;

	std::unique_ptr<CachedTexture>& tex = TextureCache[(int)masked][info->CacheID];

#if defined(OLDUNREAL469SDK)
	if (!tex || (info->bRealtimeChanged && (!info->Texture || info->Texture->RealtimeChangeCount != tex->RealtimeChangeCount)))
		UploadTexture(info, masked, tex);
#else
	if (!tex || info->bRealtimeChanged)
		UploadTexture(info, masked, tex);
#endif

	float uscale = info->UScale;
	float vscale = info->VScale;
	tex->UScale = uscale;
	tex->VScale = vscale;
	tex->PanX = info->Pan.X;
	tex->PanY = info->Pan.Y;
	tex->UMult = 1.0f / (uscale * info->USize);
	tex->VMult = 1.0f / (vscale * info->VSize);

	return tex.get();
}

void TextureManager::UploadTexture(FTextureInfo* info, bool masked, std::unique_ptr<CachedTexture>& tex)
{
	if (!tex)
	{
		tex.reset(new CachedTexture());
		renderer->Uploads->UploadTexture(tex.get(), *info, masked);
	}
#if defined(OLDUNREAL469SDK)
	else if (info->bRealtimeChanged && (!info->Texture || info->Texture->RealtimeChangeCount != tex->RealtimeChangeCount))
	{
		if (info->Texture)
			info->Texture->RealtimeChangeCount = tex->RealtimeChangeCount;
		info->bRealtimeChanged = 0;
		renderer->Uploads->UploadTexture(tex.get(), *info, masked);
	}
#else
	else if (info->bRealtimeChanged)
	{
		info->bRealtimeChanged = 0;
		renderer->Uploads->UploadTexture(tex.get(), *info, masked);
	}
#endif
}

void TextureManager::ClearCache()
{
	for (auto& cache : TextureCache)
	{
		cache.clear();
	}
}

CachedTexture* TextureManager::CreateNullTexture()
{
	NullTexture.reset(new CachedTexture());

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.Width = 1;
	texDesc.Height = 1;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	HRESULT result = renderer->Device->CreateTexture2D(&texDesc, nullptr, NullTexture->Texture.TypedInitPtr());
	ThrowIfFailed(result, "CreateTexture2D(NullTexture) failed");

	result = renderer->Device->CreateShaderResourceView(NullTexture->Texture, nullptr, NullTexture->View.TypedInitPtr());
	ThrowIfFailed(result, "CreateShaderResourceView(NullTexture) failed");

	renderer->Uploads->UploadWhite(NullTexture->Texture);

	return NullTexture.get();
}
