#pragma once

struct ID3D11Texture2D;

class CachedTexture
{
public:
	~CachedTexture()
	{
		if (View)
		{
			View->Release();
			View = nullptr;
		}
		if (Texture)
		{
			Texture->Release();
			Texture = nullptr;
		}
	}

	ID3D11Texture2D* Texture = nullptr;
	ID3D11ShaderResourceView* View = nullptr;
	int RealtimeChangeCount = 0;
	int DummyMipmapCount = 0;
};
