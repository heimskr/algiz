#pragma once

#include <filesystem>
#include <list>
#include <map>
#include <string>

#include "plugins/Plugin.h"
#include "Log.h"

namespace Algiz::Plugins {
	class PluginHost {
		public:
			using PluginTuple = std::tuple<std::string, Plugin *, void *>; // path, plugin

			template <typename T>
			// The bool argument indicates whether the result hasn't been disabled.
			using PreFn = std::function<CancelableResult(T &, bool)>;

			template <typename T>
			using PostFn = std::function<void(const T &)>;

			template <typename T>
			using PrePtr = std::weak_ptr<PreFn<T>>;

			template <typename T>
			using PostPtr = std::weak_ptr<PostFn<T>>;

		public:
			/** Determines whether a pre-event should go through. */
			template <typename T, typename C>
			bool before(T &obj, const C &funcs) {
				return beforeMulti(obj, funcs).first;
			}

			/** Determines whether a pre-event should go through. Used inside functions that process sets of sets of
			 *  handler functions. */
			template <typename T, typename C>
			std::pair<bool, HandlerResult> beforeMulti(T &obj, const C &funcs,
			bool initial = true) {
				bool should_pass = initial;
				for (auto &func: funcs) {
					if (func.expired()) {
						WARN("beforeMulti: pointer is expired");
						continue;
					}

					Plugins::CancelableResult result = (*func.lock())(obj, should_pass);

					if (result == Plugins::CancelableResult::Kill || result == Plugins::CancelableResult::Disable) {
						should_pass = false;
					} else if (result == Plugins::CancelableResult::Approve
					        || result == Plugins::CancelableResult::Enable) {
						should_pass = true;
					}

					if (result == Plugins::CancelableResult::Kill || result == Plugins::CancelableResult::Approve)
						return {should_pass, HandlerResult::Kill};
				}

				return {should_pass, HandlerResult::Pass};
			}

			template <typename T>
			void after(const T &obj, const std::list<PostPtr<T>> &funcs) {
				for (auto &func: funcs) {
					if (func.expired()) {
						WARN("after: pointer is expired");
					} else
						(*func.lock())(obj);
				}
			}

		private:
			std::list<PluginTuple> plugins {};

		public:
			virtual ~PluginHost() {}

			/** Unloads a plugin. */
			void unloadPlugin(PluginTuple &);

			/** Unloads all plugins. */
			void unloadPlugins();

			/** Loads a plugin from a given shared object. */
			PluginTuple loadPlugin(const std::string &path);

			/** Loads all plugins in a given directory. */
			void loadPlugins(const std::string &path);

			/** Returns a pointer to a plugin tuple by path or name. Returns nullptr if no match was found. */
			PluginTuple * getPlugin(const std::string &, bool insensitive = false);

			/** Returns a pointer to a plugin's tuple. Returns nullptr if no match was found. */
			PluginTuple * getPlugin(const Plugins::Plugin *);

			bool hasPlugin(const std::filesystem::path &) const;

			bool hasPlugin(const std::string &name, bool insensitive = false) const;

			/** Returns a const reference to the plugin list. */
			const std::list<PluginTuple> & getPlugins() const;

			/** Initializes all loaded plugins before client initialization. */
			void preinitPlugins();

			/** Initializes all loaded plugins after client initialization. */
			void postinitPlugins();

			/** If a plugin was loaded from a given path, a pointer to its corresponding plugin object is returned. */
			Plugins::Plugin * pluginForPath(const std::string &path) const;

			template <typename T>
			static bool erase(std::list<T> &list, const T &item) {
				auto locked = item.lock();

				for (auto iter = list.begin(), end = list.end(); iter != end; ++iter) {
					if (iter->lock() == locked) {
						list.erase(iter);
						return true;
					}
				}

				return false;
			}
	};
}
