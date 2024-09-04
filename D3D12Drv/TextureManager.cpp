
#include "Precomp.h"
#include "TextureManager.h"
#include "UD3D12RenderDevice.h"
#include "CachedTexture.h"

TextureManager::TextureManager(UD3D12RenderDevice* renderer) : renderer(renderer)
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

	if (info->Texture && (info->Texture->PolyFlags & PF_Masked))
		masked = true;

	if (info->Format != TEXF_P8)
		masked = false;

	std::unique_ptr<CachedTexture>& tex = TextureCache[(int)masked][info->CacheID];
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
	return tex.get();
}

void TextureManager::ClearCache()
{
	for (auto& cache : TextureCache)
	{
		cache.clear();
	}
}

CachedTexture* TextureManager::GetNullTexture()
{
	if (NullTexture)
		return NullTexture.get();

	NullTexture.reset(new CachedTexture());

	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Width = 1;
	texDesc.Height = 1;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;

	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	HRESULT result = renderer->MemAllocator->CreateResource(&allocationDesc, &texDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, &NullTexture->Allocation, NullTexture->Texture.GetIID(), NullTexture->Texture.InitPtr());
	ThrowIfFailed(result, "CreateResource(NullTexture) failed");

	renderer->Uploads->UploadWhite(NullTexture->Texture);

	return NullTexture.get();
}
