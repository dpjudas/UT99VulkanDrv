
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
		return nullptr;

	std::unique_ptr<CachedTexture>& tex = TextureCache[(int)masked][info->CacheID];
	if (!tex)
	{
		tex.reset(new CachedTexture());
		renderer->Uploads->UploadTexture(tex.get(), *info, masked);
	}
	else if (info->bRealtimeChanged && (!info->Texture || info->Texture->RealtimeChangeCount != tex->RealtimeChangeCount))
	{
		if (info->Texture)
			info->Texture->RealtimeChangeCount = tex->RealtimeChangeCount;
		info->bRealtimeChanged = 0;
		renderer->Uploads->UploadTexture(tex.get(), *info, masked);
	}
	return tex.get();
}

void TextureManager::ClearCache()
{
	for (auto& cache : TextureCache)
	{
		cache.clear();
	}
}
