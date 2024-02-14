
#include "Precomp.h"

IMPLEMENT_PACKAGE(D3D12Drv);

#ifdef _MSC_VER
#pragma comment(lib, "Core.lib")
#pragma comment(lib, "Engine.lib")
#if defined(OLDUNREAL469SDK)
#pragma comment(lib, "Render.lib")
#endif
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DXGI.lib")
#endif
