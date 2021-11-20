#pragma once

#include <vector>

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#pragma pack(push, 8)
#include <Windows.h>
#undef min
#undef max
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
