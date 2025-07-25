#pragma once

#include "threading/MTQueue.h"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Algiz {
	class ThreadPool {
		public:
			using Function = std::function<void(ThreadPool &, size_t)>;

			ThreadPool(size_t size);

			~ThreadPool();

			inline bool isActive() const { return active; }
			inline auto getSize() const { return size; }

			void start();
			void join();
			void detach();
			/** Returns true if the pool is active and added the job, or false if the pool is inactive. */
			bool add(const Function &);

			size_t jobCount() const;

		protected:
			const size_t size;
			MTQueue<Function> workQueue;
			std::condition_variable workCV;
			std::mutex workMutex;
			std::vector<std::thread> pool;
			std::atomic_bool active = false;
			std::atomic_bool joining = false;
			std::atomic_bool newJobReady = false;
			std::atomic_size_t jobsDone = 0;
	};
}
