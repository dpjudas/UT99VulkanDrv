#pragma once

#include <map>

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

private:
	UD3D11RenderDevice* renderer = nullptr;
	std::map<QWORD, std::unique_ptr<CachedTexture>> TextureCache[2];
};
