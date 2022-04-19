#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/HttpFileserv.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Util.h"
#include "Log.h"

namespace Algiz::Plugins {
	void HttpFileserv::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).handlers.push_back(handler);
	}

	void HttpFileserv::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).handlers, std::weak_ptr(handler));
	}

	CancelableResult HttpFileserv::handle(const HTTP::Server::HandlerArgs &args, bool non_disabled) {
		if (!non_disabled)
			return CancelableResult::Pass;

		auto &[server, client, path] = args;
		auto full_path = (server.webRoot / ("./" + path)).lexically_normal();

		if (!isSubpath(server.webRoot, full_path)) {
			ERROR("Not subpath of " << server.webRoot << ": " << full_path);
			return CancelableResult::Pass;
		}

		if (std::filesystem::is_regular_file(full_path)) {
			// Use the path as-is.
		} else {
			bool found = false;
			for (const auto &default_filename: getDefaults(server)) {
				auto new_path = full_path / default_filename;
				if (std::filesystem::is_regular_file(new_path)) {
					full_path = std::move(new_path);
					found = true;
					break;
				}
			}

			if (!found)
				return CancelableResult::Pass;
		}

		try {
			const std::string mime = getMIME(full_path.extension());
			server.send(client.id, HTTP::Response(200, readFile(full_path)).setMIME(mime), true);
			server.removeClient(client.id);
			return CancelableResult::Approve;
		} catch (std::exception &err) {
			ERROR(err.what());
			server.send(client.id, HTTP::Response(403, "Forbidden"), true);
			server.removeClient(client.id);
			return CancelableResult::Kill;
		}

		return CancelableResult::Pass;
	}

	std::vector<std::string> HttpFileserv::getDefaults(const HTTP::Server &server) {
		const auto &json = server.options.jsonObject;
		if (!json.contains("httpfileserv"))
			return {};
		const auto &config = json.at("httpfileserv");
		if (config.contains("default")) {
			const auto &defaults = config.at("default");
			if (defaults.is_string())
				return {defaults};
			if (!defaults.is_array())
				throw std::runtime_error("httpfileserv.default is required to be a string or array of strings");
			return defaults;
		}
		return {};
	}
}

Algiz::Plugins::HttpFileserv ext_plugin;
