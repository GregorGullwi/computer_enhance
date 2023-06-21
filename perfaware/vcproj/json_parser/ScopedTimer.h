#pragma once

#include <string_view>
#include "listing_0074.cpp"

class ScopedTimer
{
public:
	static inline u64 EstimatedCpuFrequency = EstimateCPUTimerFreq();

	ScopedTimer()
	{		
	}

	~ScopedTimer()
	{
		if (!m_stopped)
			Stop();
	}

	void Start()
	{
		m_stopped = false;
		m_startTime = ReadCPUTimer();
	}

	void Stop()
	{
		u64 endTime = ReadCPUTimer();
		m_elapsed = endTime - m_startTime;
		m_stopped = true;
	}

	u64 GetElapsedCycles() const
	{
		return m_elapsed;
	}

	f64 GetElapsedSeconds() const
	{
		return m_elapsed / (f64)EstimatedCpuFrequency;
	}

private:
	bool m_stopped = true;
	u64 m_startTime = 0;
	u64 m_elapsed = 0;
};