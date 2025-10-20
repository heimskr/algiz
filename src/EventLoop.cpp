#include "EventLoop.h"

namespace Algiz {
	EventLoop::EventLoop() = default;

	void EventLoop::start() {
		if (running) {
			throw std::runtime_error("Event loop already running");
		}

		stopping = false;
		running = true;
		thread = std::thread(std::bind(&EventLoop::loop, this));
	}

	void EventLoop::stop() {
		if (!running) {
			throw std::runtime_error("Event loop not running");
		}

		stopping = true;
		cv.notify_all();
		thread.join();
		running = false;
	}

	bool EventLoop::schedule(std::chrono::system_clock::time_point point, Action action) {
		if (stopping || !running) {
			return false;
		}

		std::unique_lock lock{actionsMutex};
		actions.emplace(point, std::move(action));
		cv.notify_all();
		return true;
	}

	void EventLoop::loop() {
		while (running && !stopping) {
			std::unique_lock lock{actionsMutex};

			if (actions.empty()) {
				lock.unlock();
				awaitNewAction();
				continue;
			}

			auto iter = actions.begin();
			std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

			// If the action isn't ready yet, wait for it.
			if (now < iter->first) {
				awaitNextAction(iter->first);
				if (stopping) {
					break;
				}
			}

			auto moved_action = std::move(iter->second);
			actions.erase(iter);

			// Briefly release the lock in case the called action needs to schedule more actions.
			lock.unlock();
			moved_action();
		}
	}

	void EventLoop::awaitNewAction() {
		std::unique_lock cv_lock{cvMutex};
		cv.wait(cv_lock, [&] {
			std::unique_lock actions_lock{actionsMutex};
			return stopping || !actions.empty();
		});
	}

	void EventLoop::awaitNextAction(std::chrono::system_clock::time_point point) {
		std::unique_lock cv_lock{cvMutex};
		cv.wait_for(cv_lock, point - std::chrono::system_clock::now(), [&] {
			std::unique_lock actions_lock{actionsMutex};
			return stopping || point <= std::chrono::system_clock::now();
		});
	}
}
