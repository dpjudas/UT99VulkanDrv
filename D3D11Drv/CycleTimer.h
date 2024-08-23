#pragma once

#include <cstdint>
#include <intrin.h>

class CycleTimer
{
public:
	static void SetActive(bool active);

	void Reset()
	{
		Counter = 0;
	}

	void Clock()
	{
		if (Active)
		{
			int64_t value = __rdtsc();
			Counter -= value;
		}
	}

	void Unclock()
	{
		if (Active)
		{
			int64_t value = __rdtsc();
			Counter += value;
		}
	}

	double Time()
	{
		return Counter * CountsPerSecond;
	}

	double TimeMS()
	{
		return Counter * CountsPerMillisecond;
	}

private:
	int64_t Counter = 0;

	static bool Active;
	static double CountsPerSecond;
	static double CountsPerMillisecond;
};
