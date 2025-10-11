#include "plugins/fileserv/ModuleCache.h"

#include <algorithm>
#include <dlfcn.h>

namespace Algiz::Plugins {
	ModuleCache::ModuleCache(size_t maxSize):
		maxSize(maxSize) {}

	ModuleCache::~ModuleCache() {
		std::unique_lock lock{cacheMutex};
		for (auto &[path, item]: cache) {
			item.close();
		}
	}

	std::pair<ModuleFunction, std::unique_lock<std::mutex>> ModuleCache::operator[](const std::filesystem::path &path) {
		{
			std::shared_lock lock{cacheMutex};
			if (auto iter = cache.find(path); iter != cache.end()) {
				return iter->second.pair();
			}
		}

		std::unique_lock lock{cacheMutex};
		// Might have appeared between unlocking and relocking.
		if (auto iter = cache.find(path); iter != cache.end()) {
			return iter->second.pair();
		}

		if (cache.size() >= maxSize) {
			makeSpace();
		}

		void *handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
		if (handle == nullptr) {
			throw std::runtime_error("Couldn't dlopen " + path.string());
		}

		return cache.try_emplace(path, handle).first->second.pair();
	}

	void ModuleCache::makeSpace() {
		if (cache.empty() || cache.size() < maxSize) {
			return;
		}

		using pair = decltype(cache)::value_type;

		auto iter = std::ranges::min_element(cache, [](const pair &pair1, const pair &pair2) -> bool {
			return pair1.second.lastUsed < pair2.second.lastUsed;
		});

		if (iter != cache.end()) {
			iter->second.close();
			cache.erase(iter);
		}
	}

	ModuleCache::Item::Item(void *handle):
		lastUsed(std::chrono::system_clock::now()),
		handle(handle),
		function(reinterpret_cast<ModuleFunction>(dlsym(handle, "algizModule"))) {
			if (handle == nullptr) {
				throw std::runtime_error("Module handle is null");
			}

			if (function == nullptr) {
				throw std::runtime_error("Couldn't find algizModule symbol");
			}
		}

	void ModuleCache::Item::close() {
		std::unique_lock lock{mutex};
		dlclose(handle);
	}

	std::pair<ModuleFunction, std::unique_lock<std::mutex>> ModuleCache::Item::pair() {
		return {function, std::unique_lock{mutex}};
	}
}
