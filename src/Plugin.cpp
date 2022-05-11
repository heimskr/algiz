#include "plugins/Plugin.h"
#include "plugins/PluginHost.h"

namespace Algiz::Plugins {
	bool Plugin::unload() const {
		if (parent == nullptr)
			throw std::runtime_error("Plugin host is null");

		if (PluginHost::PluginTuple *tuple = parent->getPlugin(this)) {
			parent->unloadPlugin(*tuple);
			return true;
		}

		return false;
	}

	nlohmann::json & Plugin::getConfig() {
		return config;
	}

	const nlohmann::json & Plugin::getConfig() const {
		return config;
	}

	Plugin & Plugin::setConfig(const nlohmann::json &config_) {
		config = config_;
		return *this;
	}

	Plugin & Plugin::setConfig(nlohmann::json &&config_) {
		config = std::move(config_);
		return *this;
	}
}
