/**
 * @file r8bbase.cpp
 *
 * @brief C++ file that should be compiled and included into your application.
 *
 * This is a single library file that should be compiled and included into the
 * project that uses the "r8brain-free-src" sample rate converter. This file
 * defines several global static objects used by the library.
 *
 * You may also need to include to your project: the "Kernel32" library
 * (on Windows) and the "pthread" library on Mac OS X and Linux.
 *
 * r8brain-free-src Copyright (c) 2013-2019 Aleksey Vaneev
 * See the "License.txt" file for license.
 */

#include "Precomp.h"
#include "CDSPFIRFilter.h"
#include "CDSPFracInterpolator.h"

namespace r8b {

#if R8B_FLTTEST
	int InterpFilterFracs = -1;
	int InterpFilterFracsThird = -1;
#endif // R8B_FLTTEST

CSyncObject CDSPRealFFTKeeper :: StateSync;
CDSPRealFFT :: CObjKeeper CDSPRealFFTKeeper :: FFTObjects[ 31 ];

CSyncObject CDSPFIRFilterCache :: StateSync;
CPtrKeeper< CDSPFIRFilter* > CDSPFIRFilterCache :: Objects;
int CDSPFIRFilterCache :: ObjCount = 0;

CSyncObject CDSPFracDelayFilterBankCache :: StateSync;
CPtrKeeper< CDSPFracDelayFilterBank* > CDSPFracDelayFilterBankCache :: Objects;
CPtrKeeper< CDSPFracDelayFilterBank* > CDSPFracDelayFilterBankCache :: StaticObjects;
int CDSPFracDelayFilterBankCache :: ObjCount = 0;

} // namespace r8b
