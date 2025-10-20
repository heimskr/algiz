#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

namespace Algiz {
	class EventLoop {
		public:
			using Action = std::move_only_function<void()>;

			EventLoop();

			void start();
			void stop();
			/** Returns whether the action was scheduled. Scheduling fails if the loop is stopping or not running. */
			bool schedule(std::chrono::system_clock::time_point, Action);

			template <typename D>
			bool delay(D duration, Action action) {
				return schedule(std::chrono::system_clock::now() + duration, std::move(action));
			}

		private:
			std::thread thread;
			std::multimap<std::chrono::system_clock::time_point, Action> actions;
			std::mutex actionsMutex;
			std::condition_variable cv;
			std::mutex cvMutex;
			std::atomic_bool stopping = false;
			bool running = false;

			void loop();
			void awaitNewAction();
			void awaitNextAction(std::chrono::system_clock::time_point);
	};
}
