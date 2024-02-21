#pragma once

#include "Descriptors.h"

class CachedTexture
{
public:
	ComPtr<ID3D12Resource> Texture;

	int RealtimeChangeCount = 0;
	int DummyMipmapCount = 0;
};
