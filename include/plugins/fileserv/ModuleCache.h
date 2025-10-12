#pragma once

#include "http/Server.h"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace Algiz::Plugins {
	using ModuleFunction = void (*)(Algiz::HTTP::Server::HandlerArgs &);

	class ModuleCache {
		public:
			ModuleCache(size_t maxSize);
			~ModuleCache();

			std::pair<ModuleFunction, std::shared_lock<std::shared_mutex>> operator[](const std::filesystem::path &);
			bool remove(const std::filesystem::path &);

		private:
			struct Item {
				Item(void *handle);

				std::chrono::system_clock::time_point lastUsed;
				void *handle = nullptr;
				ModuleFunction function = nullptr;
				std::shared_mutex mutex;

				void close();
				std::pair<ModuleFunction, std::shared_lock<std::shared_mutex>> pair();
			};

			size_t maxSize{};
			std::shared_mutex cacheMutex;
			std::unordered_map<std::filesystem::path, Item> cache;

			void makeSpace();
	};
}
