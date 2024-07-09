#pragma once

class CachedTexture
{
public:
	ComPtr<ID3D11Texture2D> Texture;
	ComPtr<ID3D11ShaderResourceView> View;
	int RealtimeChangeCount = 0;
	int DummyMipmapCount = 0;
};
