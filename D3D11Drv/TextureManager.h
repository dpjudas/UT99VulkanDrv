#pragma once

#include <unordered_map>

struct FTextureInfo;
class UD3D11RenderDevice;
class CachedTexture;

class TextureManager
{
public:
	TextureManager(UD3D11RenderDevice* renderer);
	~TextureManager();

	void UpdateTextureRect(FTextureInfo* info, int x, int y, int w, int h);
	CachedTexture* GetTexture(FTextureInfo* info, bool masked);

	void ClearCache();
	int GetTexturesInCache() { return TextureCache[0].size() + TextureCache[1].size(); }

	CachedTexture* GetNullTexture()
	{
		if (NullTexture)
			return NullTexture.get();
		return CreateNullTexture();
	}

private:
	void UploadTexture(FTextureInfo* info, bool masked, CachedTexture* tex);
	CachedTexture* CreateNullTexture();

	CachedTexture* GetFromCache(int masked, QWORD cacheID);

	UD3D11RenderDevice* renderer = nullptr;
	std::unordered_map<QWORD, std::unique_ptr<CachedTexture>> TextureCache[2];
	std::unique_ptr<CachedTexture> NullTexture;

	std::pair<QWORD, CachedTexture*> LastTextureResult[2];
};
