#pragma once

#include "VulkanObjects.h"

struct FTextureInfo;
struct FMipmapBase;
enum ETextureFormat;

class TextureUploader
{
public:
	virtual ~TextureUploader() = default;
	virtual void Begin(const FTextureInfo& Info, bool masked) {}
	virtual void End() {}
	virtual int GetMipSize(FMipmapBase* mip) = 0;
	virtual void UploadMip(FMipmapBase* mip, void* dst);
	virtual VkFormat GetVkFormat() = 0;

	static TextureUploader* GetUploader(ETextureFormat format);
};

class TextureUploader_P8 : public TextureUploader
{
public:
	void Begin(const FTextureInfo& Info, bool masked) override;
	int GetMipSize(FMipmapBase* mip) override;
	void UploadMip(FMipmapBase* mip, void* dst) override;
	VkFormat GetVkFormat() override;

private:
	FColor NewPal[256];
};

class TextureUploader_BGRA8_LM : public TextureUploader
{
public:
	int GetMipSize(FMipmapBase* mip) override;
	void UploadMip(FMipmapBase* mip, void* dst) override;
	VkFormat GetVkFormat() override;
};

class TextureUploader_Simple : public TextureUploader
{
public:
	TextureUploader_Simple(VkFormat format, int bytesPerPixel) : Format(format), BytesPerPixel(bytesPerPixel) { }
	int GetMipSize(FMipmapBase* mip) override { return mip->USize * mip->VSize * BytesPerPixel; }
	VkFormat GetVkFormat() override { return Format; }

private:
	VkFormat Format;
	int BytesPerPixel;
};

class TextureUploader_4x4Block : public TextureUploader
{
public:
	TextureUploader_4x4Block(VkFormat format, int bytesPerBlock) : Format(format), BytesPerBlock(bytesPerBlock) { }
	int GetMipSize(FMipmapBase* mip) override { return((mip->USize + 3) / 4) * ((mip->VSize + 3) / 4) * BytesPerBlock; }
	VkFormat GetVkFormat() override { return Format; }

private:
	VkFormat Format;
	int BytesPerBlock;
};
