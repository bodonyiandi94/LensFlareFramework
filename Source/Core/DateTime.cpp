#include "PCH.h"
#include "DateTime.h"
#include "LibraryExtensions/TabulateEx.h"

namespace DateTime
{
	////////////////////////////////////////////////////////////////////////////////
	Timer::Timer(bool autoStart)
	{
		if (autoStart) start();
	}

	////////////////////////////////////////////////////////////////////////////////
	bool Timer::start(bool flushAccumulatedTime)
	{
		bool wasRunning = m_isRunning;
		m_isRunning = true;
		m_startTime = glfwGetTime();
		if (flushAccumulatedTime)
			m_accumulatedTime = 0.0;
		return wasRunning;
	}

	////////////////////////////////////////////////////////////////////////////////
	bool Timer::stop()
	{
		bool wasRunning = m_isRunning;
		m_isRunning = false;
		m_endTime = glfwGetTime();
		m_accumulatedTime += m_endTime - m_startTime;
		return wasRunning;
	}

	////////////////////////////////////////////////////////////////////////////////
	double Timer::getElapsedTime() const
	{
		return m_accumulatedTime;
	}

	////////////////////////////////////////////////////////////////////////////////
	double Timer::getAvgTime(size_t numComputations) const
	{
		return getElapsedTime() / std::max(size_t(1), numComputations);
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string Timer::getElapsedTime(TimeUnit minTimeUnit) const
	{
		return getDeltaTimeFormatted(getElapsedTime(), minTimeUnit);
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string Timer::getAvgTime(size_t numComputations, TimeUnit minTimeUnit) const
	{
		return getDeltaTimeFormatted(getAvgTime(numComputations), minTimeUnit);
	}

	////////////////////////////////////////////////////////////////////////////////
	ScopedTimer::ScopedTimer(Debug::DebugOutputLevel logLevel, size_t numComputations, TimeUnit minTimeUnit, std::string const& computationName, Timer* timer):
		m_outputLevel(logLevel),
		m_computationName(computationName),
		m_numComputations(numComputations),
		m_minTimeUnit(minTimeUnit),
		m_defaultTimer(false),
		m_timer(timer != nullptr ? timer : &m_defaultTimer)
	{
		Debug::log_output(m_outputLevel)
			<< "Computation (" << m_computationName << ", " << m_numComputations << " computations" << ") "
			<< "started."
			<< Debug::end;

		m_timer->start();
	}

	////////////////////////////////////////////////////////////////////////////////
	ScopedTimer::~ScopedTimer()
	{
		if (m_timer)
		{
			m_timer->stop();

			Debug::log_output(m_outputLevel)
				<< "Computation (" << m_computationName << ", " << m_numComputations << " computations" << ") "
				<< "finished in " << m_timer->getElapsedTime(m_minTimeUnit) << ", "
				<< "average time per entry: " << m_timer->getAvgTime(m_numComputations, m_minTimeUnit)
				<< Debug::end;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	TimerSet::TimerSet(Debug::DebugOutputLevel logLevel, TimeUnit minTimeUnit):
		m_outputLevel(logLevel),
		m_minTimeUnit(minTimeUnit)
	{}

	////////////////////////////////////////////////////////////////////////////////
	ScopedTimer TimerSet::startComputation(std::string const& computationName, size_t numComputations)
	{
		m_computations.emplace_back(Computation{ computationName, numComputations });
		return ScopedTimer(m_outputLevel, numComputations, m_minTimeUnit, computationName, &(m_computations.back().m_timer));
	}

	////////////////////////////////////////////////////////////////////////////////
	void TimerSet::displaySummary() const
	{
		displaySummary(m_outputLevel, m_minTimeUnit);
	}

	////////////////////////////////////////////////////////////////////////////////
	void TimerSet::displaySummary(Debug::DebugOutputLevel logLevel, TimeUnit minTimeUnit) const
	{
		// Create a sorted local copy of the computations
		std::vector<Computation> computations(m_computations.begin(), m_computations.end());
		std::sort(computations.begin(), computations.end(), [](Computation const& a, Computation const& b) { return a.m_timer.getElapsedTime() > b.m_timer.getElapsedTime(); });

		// Tabulate the list of computations
		tabulate::Table table;
		table.add_row({ "Computation", "Num. computations", "Total", "Avg. per Computation" });
		for (auto const& computation : computations)
		{
			table.add_row
			({ 
				computation.m_name,
				std::to_string(computation.m_numComputations),
				std::to_string(computation.m_timer.getElapsedTime(minTimeUnit)),
				std::to_string(computation.m_timer.getAvgTime(computation.m_numComputations, minTimeUnit)),
			});
		}
		
		// Apply formatting to the table
		tabulate::formats::psql(table);

		Debug::log_output(logLevel) << "Computation timings summary:" << Debug::end;
		Debug::log_output(logLevel) << table << Debug::end;
	}

	////////////////////////////////////////////////////////////////////////////////
	const char* timeFormatDisplay()
	{
		static const char* s_format = "%H:%M:%S";
		return s_format;
	}

	////////////////////////////////////////////////////////////////////////////////
	const char* dateFormatFilename()
	{
		static const char* s_format = "%G-%m-%d-%H-%M-%S";
		return s_format;
	};

	////////////////////////////////////////////////////////////////////////////////
	const char* dateFormatDisplay()
	{
		static const char* s_format = "%G. %B %d., %H:%M:%S";
		return s_format;
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string getDateStringUtf8(const tm* t, std::string const& format)
	{
		char buffer[128];
		wchar_t wbuffer[128];
		strftime(buffer, ARRAYSIZE(buffer), format.c_str(), t);
		MultiByteToWideChar(CP_ACP, 0, buffer, -1, wbuffer, ARRAYSIZE(wbuffer));
		WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, buffer, ARRAYSIZE(buffer), NULL, NULL);
		return std::string(buffer);
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string getDateStringUtf8(time_t t, std::string const& format)
	{
		return getDateStringUtf8(localtime(&t), format);
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string getDateStringUtf8(std::string const& format)
	{
		time_t rawtime;
		time(&rawtime);
		return getDateStringUtf8(rawtime, format);
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string getDateString(const tm* t, std::string const& format)
	{
		char buffer[128];
		strftime(buffer, ARRAYSIZE(buffer), format.c_str(), t);
		return std::string(buffer);
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string getDateString(time_t t, std::string const& format)
	{
		return getDateString(localtime(&t), format);
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string getDateString(std::string const& format)
	{
		time_t rawtime;
		time(&rawtime);
		return getDateString(rawtime, format);
	}

	////////////////////////////////////////////////////////////////////////////////
	std::unordered_map<TimeUnit, std::string> s_unitTexts =
	{
		{ Years, "years" },
		{ Days, "days" },
		{ Hours, "hours" },
		{ Minutes, "minutes" },
		{ Seconds, "seconds" },
		{ Milliseconds, "milliseconds" },
		{ Microseconds, "microseconds" },
		{ Nanoseconds, "nanoseconds" },
	};

	////////////////////////////////////////////////////////////////////////////////
	std::unordered_map<TimeUnit, double> s_unitsInSeconds =
	{
		{ Nanoseconds, 1.0 / 1e9 },
		{ Microseconds, 1.0 / 1e6 },
		{ Milliseconds, 1.0 / 1e3 },
		{ Seconds, 1.0 },
		{ Minutes, 60.0 },
		{ Hours, 60.0 * 60.0 },
		{ Days, 24.0 * 60.0 * 60.0 },
		{ Years, 365.0 * 24.0 * 60.0 * 60.0 },
	};

	////////////////////////////////////////////////////////////////////////////////
	std::string getDeltaTimeFormatted(double delta, TimeUnit minTimeUnit)
	{
		// Go over the requested time units
		std::stringstream result;
		std::string separator;
		bool fullProcessed = false;
		for (size_t unit = TimeUnit_meta.members.size() - 1; !fullProcessed && unit != size_t(-1); --unit)
		{
			const TimeUnit timeUnit = TimeUnit(unit);

			// Compute the whole time units
			const int wholeTimeUnits = int(delta / s_unitsInSeconds[timeUnit]);

			// Keep the full units for the smallets time unit
			const bool keepFull = (timeUnit <= minTimeUnit && wholeTimeUnits > 0) || unit == Nanoseconds;
			fullProcessed |= keepFull;

			// Convert the seconds to the current time unit
			const double timeUnits = (keepFull) ? delta / s_unitsInSeconds[timeUnit] : double(wholeTimeUnits);

			// Remove the units from the delta
			delta -= wholeTimeUnits * s_unitsInSeconds[timeUnit];

			// Omit empty units
			if (wholeTimeUnits == 0 && !keepFull) continue;

			// Add to the result
			result << separator << timeUnits << " " << s_unitTexts[timeUnit];

			// Set the separator
			separator = ", ";
		}

		// Return the result
		return result.str();
	}
}