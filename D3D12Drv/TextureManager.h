#pragma once

#include <unordered_map>

struct FTextureInfo;
class UD3D12RenderDevice;
class CachedTexture;

class TextureManager
{
public:
	TextureManager(UD3D12RenderDevice* renderer);
	~TextureManager();

	void UpdateTextureRect(FTextureInfo* info, int x, int y, int w, int h);
	CachedTexture* GetTexture(FTextureInfo* info, bool masked);

	void ClearCache();
	int GetTexturesInCache() { return TextureCache[0].size() + TextureCache[1].size(); }

	CachedTexture* GetNullTexture();

private:
	UD3D12RenderDevice* renderer = nullptr;
	std::unordered_map<QWORD, std::unique_ptr<CachedTexture>> TextureCache[2];
	std::unique_ptr<CachedTexture> NullTexture;
};
