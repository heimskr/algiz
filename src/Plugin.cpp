#include "plugins/Plugin.h"
#include "plugins/PluginHost.h"

namespace Algiz::Plugins {
	bool Plugin::unload() {
		if (!host) {
			throw std::runtime_error("Plugin host is null");
		}

		if (PluginHost::PluginTuple *tuple = host->getPlugin(this)) {
			host->unloadPlugin(*tuple);
			return true;
		}

		return false;
	}
}
