
#include "Precomp.h"
#include "CycleTimer.h"

void CycleTimer::SetActive(bool active)
{
	Active = active;

	if (active && CountsPerSecond == 0.0)
	{
		LARGE_INTEGER countsPerSecond = {};
		QueryPerformanceFrequency(&countsPerSecond);
		if (countsPerSecond.QuadPart == 0)
			return;

		// Try to minimize the chance of a task switch during the measurement
		SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

		// Measure how many clocks we get spinning for 50 milliseconds:

		LARGE_INTEGER startTime = {};
		QueryPerformanceCounter(&startTime);
		int64_t measureEndTime = startTime.QuadPart + 50 * countsPerSecond.QuadPart / 1000;

		uint64_t startCount = __rdtsc();
		LARGE_INTEGER endTime = {};
		while (true)
		{
			QueryPerformanceCounter(&endTime);
			if (endTime.QuadPart >= measureEndTime)
				break;
		}
		uint64_t endCount = __rdtsc();

		// Restore thread priority to normal
		SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

		CountsPerSecond = (endTime.QuadPart - startTime.QuadPart) / (double)((endCount - startCount) * countsPerSecond.QuadPart);
		CountsPerMillisecond = CountsPerSecond * 1000.0;
	}
}

bool CycleTimer::Active;
double CycleTimer::CountsPerSecond;
double CycleTimer::CountsPerMillisecond;
