#pragma once

#include <functional>
#include <string>

#include "nlohmann/json.hpp"

namespace Algiz::Plugins {
	constexpr const char *PLUGIN_CREATOR_FUNCTION_NAME = "make_plugin";

	class PluginHost;

	/**
	 * Represents the priority of a plugin's handler for an event. Right now, there's no guarantee for how plugins with
	 * the same priority are ordered.
	 */
	enum class Priority: int {Low = 1, Normal = 2, High = 3};

	/**
	 * Indicates what should be done after handling an event.
	 * - `Pass` indicates that the plugin has chosen not to do anything with the event.
	 * - `Kill` indicates that propagation to other plugins should be stopped.
	 */
	enum class HandlerResult {Pass, Kill};

	/**
	 * Indicates what should be done after handling a cancelable event.
	 * - `Pass` indicates that the plugin has chosen not to do anything with the event.
	 * - `Kill` indicates that propagation to other plugins should be stopped. It also indicates that the cancelable
	 *          event shouldn't go through (like disable).
	 * - `Disable` indicates that the cancelable event shouldn't go through but continues propagation.
	 * - `Enable`  indicates that the cancelable event should go through and continues propagation.
	 * - `Approve` indicates that the cancelable event should go through (like enable) and stops propagation.
	 */
	enum class CancelableResult {Pass, Kill, Disable, Enable, Approve};

	/** Plugins modify the server's behavior. They're created by functions called "make_plugin" in shared objects. */
	struct Plugin {
		protected:
			Plugin() = default;

			nlohmann::json config;

		public:
			PluginHost *parent = nullptr;

			Plugin(const Plugin &) = delete;
			Plugin(Plugin &&) = delete;

			virtual ~Plugin() = default;

			Plugin & operator=(const Plugin &) = delete;
			Plugin & operator=(Plugin &&) = delete;

			[[nodiscard]] virtual std::string getName()        const = 0;
			[[nodiscard]] virtual std::string getDescription() const = 0;
			[[nodiscard]] virtual std::string getVersion()     const = 0;

			/** Called when the plugin first loads. */
			virtual void preinit(PluginHost *) {}

			/** Called after client initialization. */
			virtual void postinit(PluginHost *) {}

			/** Called when the client is shutting down. */
			virtual void cleanup(PluginHost *) {}

			/** Tries to unload the plugin. Returns true if the plugin was successfully unloaded. */
			bool unload() const;

			nlohmann::json & getConfig();

			const nlohmann::json & getConfig() const;

			Plugin & setConfig(const nlohmann::json &);
			Plugin & setConfig(nlohmann::json &&);
	};
}
