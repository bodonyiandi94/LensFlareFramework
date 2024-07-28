#pragma once

////////////////////////////////////////////////////////////////////////////////
//  Headers
////////////////////////////////////////////////////////////////////////////////

#include "PCH.h"
#include "Constants.h"

#include "LibraryExtensions/StdEx.h"

////////////////////////////////////////////////////////////////////////////////
/// THREADING HELPERS
////////////////////////////////////////////////////////////////////////////////
namespace Threading
{
	////////////////////////////////////////////////////////////////////////////////
	// Number of worker threads allowed to work at once
	int numThreads();

	////////////////////////////////////////////////////////////////////////////////
	// Id of the current thread
	size_t currentThreadId();

	////////////////////////////////////////////////////////////////////////////////
	template<typename Callback>
	void invoke_for_threads(int threadId, Callback callback)
	{
		for (int i = (threadId == numThreads() ? 0 : threadId); i < (threadId == numThreads() ? numThreads() : threadId + 1); ++i)
			callback(i);
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename Callback>
	void invoke_for_threads(int threadId, int maxThreads, Callback callback)
	{
		for (int i = (threadId == numThreads() ? 0 : maxThreads); i < (threadId == numThreads() ? maxThreads : threadId + 1); ++i)
			callback(i);
	}

	////////////////////////////////////////////////////////////////////////////////
	/** Structure describing the parameters for a threaded work. */
	struct ThreadedExecuteParams
	{
		// The number of threads that this work is distributed between
		size_t m_numThreads = 1;
	};

	////////////////////////////////////////////////////////////////////////////////
	/** Structure holding various information regarding the threaded execution. */
	struct ThreadedExecuteEnvironment
	{
		// How many threads we are using
		size_t m_numThreads = 0;

		// How many are running right now
		size_t m_numRunning = 0;

		// Index of the first thread that is still running
		size_t m_lowestActiveId = 0;

		// How many work items we have overall
		size_t m_numTotalWorkItems = 0;

		// How many work items we have per batch
		size_t m_numItemsPerBatch = 0;

		// Lock for status update
		std::mutex m_statusUpdateLock;

		// Mutexes to access each individual thread
		std::atomic_size_t m_nextWorkItem;

		// Whether the specified thread is running or not
		std::array<bool, Constants::s_maxThreads> m_isThreadRunning;

		// Various accessor functions
		inline bool isLeadingThread(size_t threadId = currentThreadId()) const {
			return m_lowestActiveId == threadId;
		}
		inline bool isAlive(size_t threadId = currentThreadId()) const {
			return m_isThreadRunning[threadId];
		}
	};

	////////////////////////////////////////////////////////////////////////////////
	namespace work_indices_impl
	{
		////////////////////////////////////////////////////////////////////////////////
		template<typename... S>
		struct WorkItemTypes 
		{
			using work_item_type = std::tuple<S...>;
			using work_indices_type = std::vector<work_item_type>;
			using thread_indices_type = std::vector<size_t>;
		};

		////////////////////////////////////////////////////////////////////////////////
		template<typename... S>
		struct WorkIndices
		{
			// Necessary typedefs
			using WorkIndexType = typename work_indices_impl::WorkItemTypes<S...>::work_item_type;
			using WorkIndicesType = typename work_indices_impl::WorkItemTypes<S...>::work_indices_type;
			using ThreadIndicesType = typename work_indices_impl::WorkItemTypes<S...>::thread_indices_type;

			// The number of threads that this work is distributed between
			size_t m_numThreads;

			// Number of total items (from each individual source
			WorkIndexType m_numTotalItemsPerSource;

			// Total number of invocations
			size_t m_numTotalInvocations;

			// Work item indices
			WorkIndicesType m_workIndices;

			// Individual thread indices to loop through
			ThreadIndicesType m_threadIndices;
		};

		////////////////////////////////////////////////////////////////////////////////
		template<typename S>
		size_t numWorkItems(S items)
		{
			return items;
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename S, typename... Rest>
		size_t numWorkItems(S items, Rest... rest)
		{
			return items * numWorkItems(rest...);
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename S, size_t T, size_t N>
		size_t numTotalWorkItems(S numItems, typename std::enable_if<N == T, int>::type = 0)
		{
			return std::get<N>(numItems);
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename S, size_t T, size_t N>
		size_t numTotalWorkItems(S numItems, typename std::enable_if<N != T, int>::type = 0)
		{
			return std::get<N>(numItems) * numTotalWorkItems<S, T, N + 1>(numItems);
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename S, size_t T, size_t N>
		size_t itemIndex(S indices, S numItems, typename std::enable_if<N == T, int>::type = 0)
		{
			return std::get<N>(indices);
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename S, size_t T, size_t N>
		size_t itemIndex(S indices, S numItems, typename std::enable_if<N != T, int>::type = 0)
		{
			// 0 * s1 * s2 + 1 * s2 + 2
			return numTotalWorkItems<S, T, N + 1>(numItems) * std::get<N>(indices) + itemIndex<S, T, N + 1>(indices, numItems);
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename W, size_t T, size_t N>
		void generateWorkItems(W& result, typename W::WorkIndexType base, typename std::enable_if<N == T, int>::type = 0)
		{
			// Generate the work items
			for (size_t i = 0; i < std::get<N>(result.m_numTotalItemsPerSource); ++i)
			{
				// Write out the corresponding item index
				std::get<N>(base) = i;

				// Compute the single item-index for this work item
				size_t itemId = itemIndex<typename W::WorkIndexType, T, 0>(base, result.m_numTotalItemsPerSource);

				// Store it
				result.m_workIndices.push_back(base);
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename W, size_t T, size_t N>
		void generateWorkItems(W& result, typename W::WorkIndexType base, typename std::enable_if<N != T, int>::type = 0)
		{
			// Generate the work items
			for (size_t i = 0; i < std::get<N>(result.m_numTotalItemsPerSource); ++i)
			{
				// Write out the corresponding item index
				std::get<N>(base) = i;

				// Invoke the child loop
				generateWorkItems<W, T, N + 1>(result, base);
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename... S>
		void generateWorkItems(WorkIndices<S...>& result)
		{
			using ItemType = typename work_indices_impl::WorkItemTypes<S...>::work_item_type;
			generateWorkItems<WorkIndices<S...>, sizeof...(S) - 1, 0>(result, ItemType{});
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename S>
		S clampWorkItem(S item)
		{
			return std::max(S(1), item);
			//return item <= 0 ? 1 : item;
		}
	};

	////////////////////////////////////////////////////////////////////////////////
	template<typename... S>
	work_indices_impl::WorkIndices<S...> workIndices(ThreadedExecuteParams const& params, S... workItems)
	{
		// Result of the generation
		work_indices_impl::WorkIndices<S...> result;

		// Store the execute parameters
		result.m_numThreads = std::max(size_t(1), params.m_numThreads);

		// Total number of invocations
		result.m_numTotalInvocations = work_indices_impl::numWorkItems(work_indices_impl::clampWorkItem(workItems)...);

		// Store the total number of items
		result.m_numTotalItemsPerSource = std::make_tuple(work_indices_impl::clampWorkItem(workItems)...);

		// Generate the thread indices
		result.m_threadIndices = std::iota<size_t>(result.m_numThreads, 0);

		// Actually generate the work items
		work_indices_impl::generateWorkItems<S...>(result);

		// Return the result
		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace threaded_execute_impl
	{
		////////////////////////////////////////////////////////////////////////////////
		void initExecution(ThreadedExecuteEnvironment& environment, size_t numThreads, size_t numTotalWorkItems);

		////////////////////////////////////////////////////////////////////////////////
		void cleanupExecution(ThreadedExecuteEnvironment& environment);

		////////////////////////////////////////////////////////////////////////////////
		void initWorker(ThreadedExecuteEnvironment& environment, size_t threadId);

		////////////////////////////////////////////////////////////////////////////////
		void cleanupWorker(ThreadedExecuteEnvironment& environment, size_t threadId);

		////////////////////////////////////////////////////////////////////////////////
		template<typename Fn, typename W>
		void loopCore(ThreadedExecuteEnvironment& environment, Fn const& fn, W const& indices, size_t threadId)
		{
			// Init the worker
			initWorker(environment, threadId);
			// Invoke the loop core function
			for (size_t index = environment.m_nextWorkItem++; index < environment.m_numTotalWorkItems; index = environment.m_nextWorkItem++)
				fn(environment, indices.m_workIndices[index]);
			// Release the worker
			cleanupWorker(environment, threadId);
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename Fn, typename W>
		auto wrapLoopCore(ThreadedExecuteEnvironment& environment, Fn const& fn, W const& indices)
		{
			return ;
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename EP, typename F, typename... S>
		void executeImpl(work_indices_impl::WorkIndices<S...> const& indices, F const& fn, EP execPolicy)
		{
			ThreadedExecuteEnvironment environment;
			initExecution(environment, indices.m_numThreads, indices.m_numTotalInvocations);
			std::for_each(execPolicy, indices.m_threadIndices.begin(), indices.m_threadIndices.end(), 
				[&](size_t threadId) { loopCore(environment, fn, indices, threadId); });
			cleanupExecution(environment);
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename F, typename... S>
		void execute(work_indices_impl::WorkIndices<S...> const& indices, F const& fn)
		{
			ThreadedExecuteEnvironment environment;
			initExecution(environment, indices.m_numThreads, indices.m_numTotalInvocations);
			if (indices.m_numThreads > 1)
			{
				std::for_each(std::execution::par_unseq, indices.m_threadIndices.begin(), indices.m_threadIndices.end(), 
					[&](size_t threadId) { loopCore(environment, fn, indices, threadId); });
			}
			else
			{
				loopCore(environment, fn, indices, 0);
			}
			cleanupExecution(environment);
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename F, typename... S>
		auto loopCoreIndices(F const& fn)
		{
			return [&](ThreadedExecuteEnvironment const& environment, auto const& indices)
			{
				std::apply(fn, std::tuple_cat(std::tuple<ThreadedExecuteEnvironment const&>(environment), indices));
			};
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename F, typename... S>
		void executeIndices(ThreadedExecuteParams const& params, F const& fn, S... workItems)
		{
			// Generate thread indices
			auto indices = Threading::workIndices(params, workItems...);

			// Construct the callback fn.
			auto callback = loopCoreIndices(fn);

			// Execute the loop
			execute(indices, callback);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename F, typename... S>
	void threadedExecuteIndices(ThreadedExecuteParams const& params, F const& fn, S... workItems)
	{
		threaded_execute_impl::executeIndices(params, fn, workItems...);
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename F, typename... S>
	void threadedExecuteIndices(size_t numThreads, F const& fn, S... workItems)
	{
		ThreadedExecuteParams params;
		params.m_numThreads = numThreads;
		threadedExecuteIndices(params, fn, workItems...);
	}
}