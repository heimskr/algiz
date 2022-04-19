#include "plugins/Plugin.h"
#include "plugins/PluginHost.h"

namespace Algiz::Plugins {
	bool Plugin::unload() {
		if (!parent)
			throw std::runtime_error("Plugin host is null");

		if (PluginHost::PluginTuple *tuple = parent->getPlugin(this)) {
			parent->unloadPlugin(*tuple);
			return true;
		}

		return false;
	}
}
