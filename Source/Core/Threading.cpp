#include "PCH.h"
#include "Threading.h"
#include "Config.h"
#include "Debug.h"

namespace Threading
{
	////////////////////////////////////////////////////////////////////////////////
	// Id of the current thread (thread local value)
	thread_local size_t s_currentThreadId = 0;

	////////////////////////////////////////////////////////////////////////////////
	// Number of worker threads allowed to work at once
	int numThreads()
	{
		static int s_numThreads = glm::clamp(Config::AttribValue("threads").get<int>(), 0, glm::min((int)std::thread::hardware_concurrency(), (int)Constants::s_maxThreads));
		return s_numThreads;
	}

	////////////////////////////////////////////////////////////////////////////////
	size_t currentThreadId()
	{
		return s_currentThreadId;
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace threaded_execute_impl
	{
		////////////////////////////////////////////////////////////////////////////////
		void initExecutionSeq(ThreadedExecuteEnvironment& environment, size_t numTotalWorkItems)
		{
			// Initialize the global structures
			environment.m_nextWorkItem = 0;
			environment.m_numTotalWorkItems = numTotalWorkItems;
			environment.m_numThreads = 1;
			environment.m_numRunning = 1;
			environment.m_lowestActiveId = 0;
		}

		////////////////////////////////////////////////////////////////////////////////
		void initExecutionDist(ThreadedExecuteEnvironment& environment, size_t numThreads, size_t numTotalWorkItems)
		{
			// Initialize the global structures
			environment.m_nextWorkItem = 0;
			environment.m_numTotalWorkItems = numTotalWorkItems;
			environment.m_numThreads = numThreads;
			environment.m_numRunning = numThreads;
			environment.m_lowestActiveId = 0;
		}

		////////////////////////////////////////////////////////////////////////////////
		void initExecution(ThreadedExecuteEnvironment& environment, size_t numThreads, size_t numTotalWorkItems)
		{
			// Invoke the appropriate delegate
			if (numThreads <= 1) initExecutionSeq(environment, numTotalWorkItems);
			else                 initExecutionDist(environment, numThreads, numTotalWorkItems);
		}

		////////////////////////////////////////////////////////////////////////////////
		void cleanupExecutionSeq(ThreadedExecuteEnvironment& environment)
		{
			Debug::log_trace() << "Threaded work finished." << Debug::end;
		}

		////////////////////////////////////////////////////////////////////////////////
		void cleanupExecutionDist(ThreadedExecuteEnvironment& environment)
		{
			Debug::log_trace() << "Threaded work finished." << Debug::end;
		}

		////////////////////////////////////////////////////////////////////////////////
		void cleanupExecution(ThreadedExecuteEnvironment& environment)
		{
			// Invoke the appropriate delegate
			if (environment.m_numThreads <= 1) cleanupExecutionSeq(environment);
			else                               cleanupExecutionDist(environment);
		}

		////////////////////////////////////////////////////////////////////////////////
		void initWorkerDist(ThreadedExecuteEnvironment& environment, size_t threadId)
		{
			// Set the current thread id
			s_currentThreadId = threadId;

			// Initialize the thread's attributes
			environment.m_isThreadRunning[threadId] = true;
		}

		////////////////////////////////////////////////////////////////////////////////
		void initWorkerSeq(ThreadedExecuteEnvironment& environment, size_t threadId)
		{
			// Initialize the thread's attributes
			environment.m_isThreadRunning[threadId] = true;
		}

		////////////////////////////////////////////////////////////////////////////////
		void initWorker(ThreadedExecuteEnvironment& environment, size_t threadId)
		{
			if (environment.m_numThreads <= 1) initWorkerSeq(environment, threadId);
			else                               initWorkerDist(environment, threadId);
		}

		////////////////////////////////////////////////////////////////////////////////
		void cleanupWorkerDist(ThreadedExecuteEnvironment& environment, size_t threadId)
		{
			Debug::log_trace() << "Cleaning up worker thread " << threadId << Debug::end;

			// Signal that the thread is off
			environment.m_isThreadRunning[threadId] = false;

			{
				std::lock_guard lockGuard(environment.m_statusUpdateLock);

				// Decrement the running index
				--environment.m_numRunning;

				// Update the first active thread index
				environment.m_lowestActiveId = std::numeric_limits<size_t>::max();
				for (size_t i = 0; i < environment.m_numThreads; ++i)
				{
					if (environment.m_isThreadRunning[i])
					{
						environment.m_lowestActiveId = i;
						break;
					}
				}

				Debug::log_trace() << environment.m_numRunning << " threads still running, lowest worker thread id: " << environment.m_lowestActiveId << Debug::end;
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		void cleanupWorkerSeq(ThreadedExecuteEnvironment& environment)
		{

		}

		////////////////////////////////////////////////////////////////////////////////
		void cleanupWorker(ThreadedExecuteEnvironment& environment, size_t threadId)
		{
			if (environment.m_numThreads <= 1) cleanupWorkerSeq(environment);
			else                               cleanupWorkerDist(environment, threadId);
		}
	}
}