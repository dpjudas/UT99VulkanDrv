#pragma once

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#define USE_SSE2
#endif

#ifdef WIN32

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#ifdef _MSC_VER
#pragma warning(disable: 4297) // warning C4297: 'UObject::operator delete': function assumed not to throw an exception but does
#pragma warning(disable: 4244) // warning C4244: 'argument': conversion from 'VkDescriptorSet' to 'size_t', possible loss of data
#endif

#pragma pack(push, 8)

#include <Windows.h>

#endif

#include <zvulkan/vulkandevice.h>
#include <zvulkan/vulkaninstance.h>
#include <zvulkan/vulkansurface.h>
#include <zvulkan/vulkanswapchain.h>
#include <zvulkan/vulkanbuilders.h>
#include <zvulkan/vulkancompatibledevice.h>
#include <mutex>
#include <vector>
#include <algorithm>
#include <memory>
#include <map>
#include <unordered_map>

#ifdef WIN32

#pragma pack(pop)

#define UTGLR_NO_APP_MALLOC
#include <stdlib.h>

#endif

#include "Engine.h"
#include "UnRender.h"

#if !defined(UNREALGOLD) && !defined(DEUSEX)
#define OLDUNREAL469SDK
#endif

#if defined(OLDUNREAL469SDK)
#include "Render.h"
#endif
