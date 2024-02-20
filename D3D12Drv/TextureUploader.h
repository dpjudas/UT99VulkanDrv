#pragma once

struct FTextureInfo;
struct FMipmapBase;
enum ETextureFormat;

class TextureUploader
{
public:
	TextureUploader(DXGI_FORMAT format) : Format(format) { }
	virtual ~TextureUploader() = default;

	virtual int GetUploadSize(int x, int y, int w, int h) = 0;
	virtual void UploadRect(void* dst, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked) = 0;
	virtual int GetUploadHeight(int h) { return h; }

	DXGI_FORMAT GetDxgiFormat() const { return Format; }

	static TextureUploader* GetUploader(ETextureFormat format);

private:
	DXGI_FORMAT Format;
};

class TextureUploader_P8 : public TextureUploader
{
public:
	TextureUploader_P8() : TextureUploader(DXGI_FORMAT_R8G8B8A8_UNORM) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked) override;
};

class TextureUploader_RGB8 : public TextureUploader
{
public:
	TextureUploader_RGB8() : TextureUploader(DXGI_FORMAT_R8G8B8A8_UNORM) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked) override;
};

class TextureUploader_BGRA8_LM : public TextureUploader
{
public:
	TextureUploader_BGRA8_LM() : TextureUploader(DXGI_FORMAT_R8G8B8A8_UNORM) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked) override;
};

class TextureUploader_RGB10A2 : public TextureUploader
{
public:
	TextureUploader_RGB10A2() : TextureUploader(DXGI_FORMAT_R16G16B16A16_UNORM) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked) override;
};

class TextureUploader_RGB10A2_UI : public TextureUploader
{
public:
	TextureUploader_RGB10A2_UI() : TextureUploader(DXGI_FORMAT_R16G16B16A16_UINT) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked) override;
};

class TextureUploader_RGB10A2_LM : public TextureUploader
{
public:
	TextureUploader_RGB10A2_LM() : TextureUploader(DXGI_FORMAT_R16G16B16A16_UNORM) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked) override;
};

class TextureUploader_Simple : public TextureUploader
{
public:
	TextureUploader_Simple(DXGI_FORMAT format, int bytesPerPixel) : TextureUploader(format), BytesPerPixel(bytesPerPixel) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked) override;

private:
	int BytesPerPixel;
};

class TextureUploader_4x4Block : public TextureUploader
{
public:
	TextureUploader_4x4Block(DXGI_FORMAT format, int bytesPerBlock) : TextureUploader(format), BytesPerBlock(bytesPerBlock) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked) override;
	int GetUploadHeight(int h) override { return (h + 3) / 4; }

private:
	int BytesPerBlock;
};

class TextureUploader_2DBlock : public TextureUploader
{
public:
	TextureUploader_2DBlock(DXGI_FORMAT format, int blockX, int blockY, int bytesPerBlock) : TextureUploader(format), BlockX(blockX), BlockY(blockY), BytesPerBlock(bytesPerBlock) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, FMipmapBase* mip, int x, int y, int w, int h, FColor* palette, bool masked) override;
	int GetUploadHeight(int h) override { return (h + BlockY - 1) / BlockY; }

private:
	int BlockX;
	int BlockY;
	int BytesPerBlock;
};
