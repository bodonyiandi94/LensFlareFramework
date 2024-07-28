#pragma once

////////////////////////////////////////////////////////////////////////////////
//  Headers
////////////////////////////////////////////////////////////////////////////////

#include "PCH.h"
#include "Constants.h"
#include "Debug.h"

namespace DateTime
{
	////////////////////////////////////////////////////////////////////////////////
	meta_enum(TimeUnit, int, Nanoseconds, Microseconds, Milliseconds, Seconds, Minutes, Hours, Days, Years);

	////////////////////////////////////////////////////////////////////////////////
	// Represent a timer object
	struct Timer
	{
		Timer(bool autoStart = false);

		bool start(bool flushAccumulatedTime = false);
		bool stop();

		double getElapsedTime() const;
		double getAvgTime(size_t numComputations) const;
		std::string getElapsedTime(TimeUnit minTimeUnit) const;
		std::string getAvgTime(size_t numComputations, TimeUnit minTimeUnit) const;

		// ---- Private members

		bool m_isRunning = false;
		double m_startTime = DBL_MAX;
		double m_endTime = DBL_MAX;
		double m_accumulatedTime = 0.0;
	};

	////////////////////////////////////////////////////////////////////////////////
	struct ScopedTimer
	{
		ScopedTimer(Debug::DebugOutputLevel logLevel, size_t numComputations, TimeUnit minTimeUnit, std::string const& computationName, Timer* timer = nullptr);
		ScopedTimer(ScopedTimer&& other) = default;
		~ScopedTimer();

		// ---- Private members

		Debug::DebugOutputLevel m_outputLevel;
		std::string m_computationName;
		size_t m_numComputations;
		TimeUnit m_minTimeUnit;
		Timer* m_timer;
		Timer m_defaultTimer;
	};

	////////////////////////////////////////////////////////////////////////////////
	struct TimerSet
	{
		TimerSet() = default;
		TimerSet(Debug::DebugOutputLevel logLevel, TimeUnit minTimeUnit);

		ScopedTimer startComputation(std::string const& computationName, size_t numComputations);

		void displaySummary() const;
		void displaySummary(Debug::DebugOutputLevel logLevel, TimeUnit minTimeUnit) const;

		// ---- Private members

		struct Computation
		{
			std::string m_name;
			size_t m_numComputations;
			Timer m_timer;
		};

		Debug::DebugOutputLevel m_outputLevel;
		TimeUnit m_minTimeUnit;
		std::list<Computation> m_computations;
	};

	////////////////////////////////////////////////////////////////////////////////
	/** Various commonly used date format definitions. */
	const char* timeFormatDisplay();
	const char* dateFormatFilename();
	const char* dateFormatDisplay();

	////////////////////////////////////////////////////////////////////////////////
	std::string getDateStringUtf8(const tm* t, std::string const& format);

	////////////////////////////////////////////////////////////////////////////////
	std::string getDateStringUtf8(time_t t, std::string const& format);

	////////////////////////////////////////////////////////////////////////////////
	std::string getDateStringUtf8(std::string const& format);

	////////////////////////////////////////////////////////////////////////////////
	std::string getDateString(const tm* t, std::string const& format);

	////////////////////////////////////////////////////////////////////////////////
	std::string getDateString(time_t t, std::string const& format);

	////////////////////////////////////////////////////////////////////////////////
	std::string getDateString(std::string const& format);

	////////////////////////////////////////////////////////////////////////////////
	std::string getDeltaTimeFormatted(double delta, TimeUnit minTimeUnit = Seconds);
}