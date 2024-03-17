#pragma once

#include "Descriptors.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"

class CachedTexture
{
public:
	~CachedTexture()
	{
		if (Allocation)
			Allocation->Release();
	}

	D3D12MA::Allocation* Allocation = nullptr;
	ID3D12Resource* Texture = nullptr;

	int RealtimeChangeCount = 0;
	int DummyMipmapCount = 0;
};
