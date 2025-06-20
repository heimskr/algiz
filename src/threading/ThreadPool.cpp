#include "threading/ThreadPool.h"

namespace Algiz {
	ThreadPool::ThreadPool(size_t size):
		size(size) {
			pool.reserve(size);
		}

	ThreadPool::~ThreadPool() {
		join();
	}

	void ThreadPool::start() {
		if (joining || active.exchange(true)) {
			return;
		}

		assert(pool.empty());

		for (size_t thread_index = 0; thread_index < size; ++thread_index) {
			pool.emplace_back([this, thread_index] {
				size_t last_jobs_done = jobsDone.load();
				while (active) {
					{
						std::unique_lock lock(workMutex);
						workCV.wait(lock, [this, &last_jobs_done] {
							return joining.load() || newJobReady.load() || last_jobs_done < jobsDone;
						});
					}
					if (joining) {
						break;
					}
					if (auto job = workQueue.tryTake()) {
						newJobReady = false;
						assert(*job);
						(*job)(*this, thread_index);
						last_jobs_done = ++jobsDone;
						// Issue when the thread pool contains only one worker?
						workCV.notify_one();
					} else {
						last_jobs_done = jobsDone;
					}
				}
			});
		}
	}

	void ThreadPool::join() {
		if (active.exchange(false) && !joining.exchange(true)) {
			workQueue.clear();
			workCV.notify_all();
			for (std::thread &thread: pool) {
				thread.join();
			}
			pool.clear();
			joining = false;
		}
	}

	void ThreadPool::detach() {
		if (active.exchange(false) && !joining.exchange(true)) {
			workQueue.clear();
			workCV.notify_all();
			for (std::thread &thread: pool) {
				thread.detach();
			}
			pool.clear();
			joining = false;
		}
	}

	bool ThreadPool::add(const Function &function) {
		if (active) {
			// TODO: race condition if active goes false during this block
			workQueue.push(function);
			newJobReady = true;
			workCV.notify_one();
			return true;
		}

		return false;
	}

	size_t ThreadPool::jobCount() const {
		return workQueue.size();
	}
}
