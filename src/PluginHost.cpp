#include <dlfcn.h>
#include <filesystem>

#include "plugins/PluginHost.h"
#include "util/Util.h"

namespace Algiz::Plugins {
	void PluginHost::unloadPlugin(PluginTuple &tuple) {
		std::get<1>(tuple)->cleanup(this);
		auto iter = std::find(plugins.begin(), plugins.end(), tuple);
		if (iter == plugins.end())
			throw std::runtime_error("Couldn't find plugin tuple: " + std::get<0>(tuple));
		plugins.erase(iter);
		dlclose(std::get<2>(tuple));
	}

	void PluginHost::unloadPlugins() {
		while (!plugins.empty())
			unloadPlugin(plugins.front());
		plugins.clear();
	}

	PluginHost::PluginTuple PluginHost::loadPlugin(const std::string &path) {
		const auto canonical = std::filesystem::canonical(path);

		if (!std::filesystem::exists(canonical))
			throw std::filesystem::filesystem_error("No plugin found at path",
				std::error_code(ENOENT, std::generic_category()));


		void *lib = dlopen(canonical.c_str(), RTLD_NOW | RTLD_LOCAL);
		if (lib == nullptr)
			throw std::runtime_error("dlopen returned nullptr. " + std::string(dlerror()));

		auto *make_plugin = reinterpret_cast<Plugin * (*)()>(dlsym(lib, PLUGIN_CREATOR_FUNCTION_NAME));
		if (make_plugin == nullptr)
			throw std::runtime_error("Plugin creation function is null");

		return plugins.emplace_back(canonical, std::shared_ptr<Plugin>(make_plugin()), lib);
	}

	void PluginHost::loadPlugins(const std::string &path) {
		for (const auto &entry: std::filesystem::directory_iterator(path)) {
			if (entry.is_directory()) {
				loadPlugins(entry.path());
			} else {
				const std::string extension = entry.path().extension();
				if (extension == ".so" || extension == ".dylib")
					loadPlugin(entry.path().c_str());
			}
		}
	}

	PluginHost::PluginTuple * PluginHost::getPlugin(const std::string &name, bool insensitive) {
		decltype(plugins)::iterator iter;
		if (insensitive) {
			const std::string lower = toLower(name);
			iter = std::find_if(plugins.begin(), plugins.end(), [&](const PluginTuple &tuple) {
				return std::get<0>(tuple) == name || toLower(std::get<1>(tuple)->getName()) == lower;
			});
		} else {
			iter = std::find_if(plugins.begin(), plugins.end(), [&](const PluginTuple &tuple) {
				return std::get<0>(tuple) == name || std::get<1>(tuple)->getName() == name;
			});
		}

		return iter == plugins.end()? nullptr : &*iter;
	}

	PluginHost::PluginTuple * PluginHost::getPlugin(const Plugins::Plugin *plugin) {
		auto iter = std::find_if(plugins.begin(), plugins.end(), [&](const PluginTuple &tuple) {
			return std::get<1>(tuple).get() == plugin;
		});

		return iter == plugins.end()? nullptr : &*iter;
	}

	bool PluginHost::hasPlugin(const std::filesystem::path &path) const {
		const auto canonical = std::filesystem::canonical(path);
		return plugins.end() != std::find_if(plugins.begin(), plugins.end(), [&](const PluginTuple &tuple) {
			return canonical == std::get<0>(tuple);
		});
	}

	bool PluginHost::hasPlugin(const std::string &name, bool insensitive) const {
		if (insensitive) {
			const std::string lower = toLower(name);
			return plugins.end() != std::find_if(plugins.begin(), plugins.end(), [&](const PluginTuple &tuple) {
				return toLower(std::get<1>(tuple)->getName()) == lower;
			});
		}

		return plugins.end() != std::find_if(plugins.begin(), plugins.end(), [&](const PluginTuple &tuple) {
			return std::get<1>(tuple)->getName() == name;
		});
	}

	const std::list<PluginHost::PluginTuple> & PluginHost::getPlugins() const {
		return plugins;
	}

	void PluginHost::preinitPlugins() {
		for (const PluginTuple &tuple: plugins)
			std::get<1>(tuple)->preinit(this);
	}

	void PluginHost::postinitPlugins() {
		for (const PluginTuple &tuple: plugins)
			std::get<1>(tuple)->postinit(this);
	}

	std::shared_ptr<Plugins::Plugin> PluginHost::pluginForPath(const std::string &path) const {
		auto iter = std::find_if(plugins.begin(), plugins.end(), [&](const PluginTuple &tuple) {
			return std::get<0>(tuple) == path;
		});

		return iter == plugins.end()? nullptr : std::get<1>(*iter);
	}
}
