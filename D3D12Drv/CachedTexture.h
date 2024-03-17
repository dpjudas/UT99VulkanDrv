#pragma once

#include "Descriptors.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"

class CachedTexture
{
public:
	~CachedTexture()
	{
		Texture.reset();
		if (Allocation)
			Allocation->Release();
	}

	D3D12MA::Allocation* Allocation = nullptr;
	ComPtr<ID3D12Resource> Texture;

	int RealtimeChangeCount = 0;
	int DummyMipmapCount = 0;
};
