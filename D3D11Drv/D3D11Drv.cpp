
#include "Precomp.h"

IMPLEMENT_PACKAGE(D3D11Drv);

#ifdef _MSC_VER
#pragma comment(lib, "Core.lib")
#pragma comment(lib, "Engine.lib")
#if defined(OLDUNREAL469SDK)
#pragma comment(lib, "Render.lib")
#endif
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#endif
