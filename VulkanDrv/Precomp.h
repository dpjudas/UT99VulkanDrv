#pragma once

#include <vector>

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#pragma pack(push, 8)
#include <Windows.h>
#undef min
#undef max
#include "volk/volk.h"
#include "vk_mem_alloc/vk_mem_alloc.h"
#include "../ShaderCompiler/glslang/Public/ShaderLang.h"
#include "../ShaderCompiler/spirv/GlslangToSpv.h"
#include <mutex>
#include <vector>
#include <algorithm>
#include <memory>
#pragma pack(pop)

#define UTGLR_NO_APP_MALLOC
#include <stdlib.h>

#ifdef _MSC_VER
#pragma warning(disable: 4297) // warning C4297: 'UObject::operator delete': function assumed not to throw an exception but does
#endif

#include "Engine.h"
#include "UnRender.h"
