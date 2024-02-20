#pragma once

#include "TextureUploader.h"

class UD3D12RenderDevice;
class CachedTexture;
struct FTextureInfo;

class UploadManager
{
public:
	UploadManager(UD3D12RenderDevice* renderer);
	~UploadManager();

	bool SupportsTextureFormat(ETextureFormat Format) const;

	void UploadTexture(CachedTexture* tex, const FTextureInfo& Info, bool masked);
	void UploadTextureRect(CachedTexture* tex, const FTextureInfo& Info, int x, int y, int w, int h);

	void UploadWhite(ID3D12Resource* image);

private:
	void UploadData(ID3D12Resource* image, const FTextureInfo& Info, bool masked, TextureUploader* uploader, int dummyMipmapCount, INT minSize);

	uint8_t* GetUploadBuffer(size_t size);

	UINT CalcSubresource(UINT MipSlice, UINT ArraySlice, UINT PlaneSlice, UINT MipLevels, UINT ArraySize)
	{
		return MipSlice + ArraySlice * MipLevels + PlaneSlice * MipLevels * ArraySize;
	}

	UINT CalcSubresource(UINT MipSlice, UINT ArraySlice, UINT MipLevels)
	{
		return MipSlice + ArraySlice * MipLevels;
	}

	UD3D12RenderDevice* renderer = nullptr;
	std::vector<uint32_t> UploadBuffer;
};
