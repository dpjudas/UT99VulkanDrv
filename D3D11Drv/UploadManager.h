#pragma once

#include "TextureUploader.h"

class UD3D11RenderDevice;
class CachedTexture;
struct FTextureInfo;

class UploadManager
{
public:
	UploadManager(UD3D11RenderDevice* renderer);
	~UploadManager();

	bool SupportsTextureFormat(ETextureFormat Format) const;

	void UploadTexture(CachedTexture* tex, const FTextureInfo& Info, bool masked);
	void UploadTextureRect(CachedTexture* tex, const FTextureInfo& Info, int x, int y, int w, int h);

	void SubmitUploads();

	void UploadWhite(ID3D11Texture2D* image);

private:
	void UploadData(ID3D11Texture2D* image, const FTextureInfo& Info, bool masked, TextureUploader* uploader, int dummyMipmapCount, INT minSize);

	uint8_t* GetUploadBuffer(size_t size);

	UD3D11RenderDevice* renderer = nullptr;
	std::vector<uint32_t> UploadBuffer;
};
