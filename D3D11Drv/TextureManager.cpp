
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

CachedTexture* TextureManager::GetFromCache(int masked, QWORD cacheID)
{
	if (LastTextureResult[masked].first == cacheID && LastTextureResult[masked].second)
		return LastTextureResult[masked].second;

	renderer->Timers.TextureCache.Clock();
	LastTextureResult[masked].first = cacheID;
	LastTextureResult[masked].second = TextureCache[masked][cacheID].get();
	renderer->Timers.TextureCache.Unclock();

	return LastTextureResult[masked].second;
}

void TextureManager::UpdateTextureRect(FTextureInfo* info, int x, int y, int w, int h)
{
	CachedTexture* tex = GetFromCache(0, info->CacheID);
	if (tex)
	{
		renderer->Timers.TextureUpload.Clock();
		renderer->Uploads->UploadTextureRect(tex, *info, x, y, w, h);
		info->bRealtimeChanged = 0;
		renderer->Timers.TextureUpload.Unclock();
	}
}

CachedTexture* TextureManager::GetTexture(FTextureInfo* info, bool masked)
{
	if (!info)
		return GetNullTexture();

	if (info->Texture && (info->Texture->PolyFlags & PF_Masked))
		masked = true;

	if (info->Format != TEXF_P8)
		masked = false;

	CachedTexture* tex = GetFromCache((int)masked, info->CacheID);
	if (!tex)
	{
		std::unique_ptr<CachedTexture>& tex2 = TextureCache[(int)masked][info->CacheID];
		tex2.reset(new CachedTexture());
		tex = tex2.get();

		renderer->Timers.TextureUpload.Clock();
		renderer->Uploads->UploadTexture(tex, *info, masked);
		renderer->Timers.TextureUpload.Unclock();
	}
	else
	{
#if defined(OLDUNREAL469SDK)
		if (info->bRealtimeChanged && (!info->Texture || info->Texture->RealtimeChangeCount != tex->RealtimeChangeCount))
			UploadTexture(info, masked, tex);
#else
		if (info->bRealtimeChanged)
			UploadTexture(info, masked, tex);
#endif
	}

	float uscale = info->UScale;
	float vscale = info->VScale;
	tex->UScale = uscale;
	tex->VScale = vscale;
	tex->PanX = info->Pan.X;
	tex->PanY = info->Pan.Y;
	tex->UMult = 1.0f / (uscale * info->USize);
	tex->VMult = 1.0f / (vscale * info->VSize);

	return tex;
}

void TextureManager::UploadTexture(FTextureInfo* info, bool masked, CachedTexture* tex)
{
#if defined(OLDUNREAL469SDK)
	if (info->bRealtimeChanged && (!info->Texture || info->Texture->RealtimeChangeCount != tex->RealtimeChangeCount))
	{
		renderer->Timers.TextureUpload.Clock();

		if (info->Texture)
			info->Texture->RealtimeChangeCount = tex->RealtimeChangeCount;
		info->bRealtimeChanged = 0;
		renderer->Uploads->UploadTexture(tex, *info, masked);

		renderer->Timers.TextureUpload.Unclock();
	}
#else
	else if (info->bRealtimeChanged)
	{
		renderer->Timers.TextureUpload.Clock();

		info->bRealtimeChanged = 0;
		renderer->Uploads->UploadTexture(tex, *info, masked);

		renderer->Timers.TextureUpload.Unclock();
	}
#endif
}

void TextureManager::ClearCache()
{
	for (auto& cache : TextureCache)
	{
		cache.clear();
	}
	for (auto& texture : LastTextureResult)
		texture = {};
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
