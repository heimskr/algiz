#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/Ansuz.h"
#include "plugins/ansuz/resources.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Util.h"

#include "inja.hpp"
#include "Log.h"

#define EXTERNAL_RESOURCES

#ifdef EXTERNAL_RESOURCES
#define RESOURCE(a, b) readFile("res/ansuz/" b)
#else
#define RESOURCE(a, b) std::string_view(ansuz_##a, ansuz_##a##_len)
#endif

namespace Algiz::Plugins {
	void Ansuz::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).handlers.push_back(handler);
	}

	void Ansuz::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).handlers, std::weak_ptr(handler));
	}

	CancelableResult Ansuz::handle(const HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		const auto &[http, client, request, parts] = args;

		if (!parts.empty() && parts.front() == "ansuz") {
			try {
				if (parts.size() == 1)
					return serveIndex(http, client);

				if (parts.size() == 2) {
					if (parts[1] == "bootstrap.min.css")
						return serve(http, client, RESOURCE(bootstrap_css, "bootstrap.min.css"), {}, "text/css");
					if (parts[1] == "bootstrap-utilities.min.css")
						return serve(http, client, RESOURCE(bootstrap_util_css, "bootstrap-utilities.min.css"), {},
							"text/css");
					if (parts[1] == "load") {
						std::vector<std::pair<std::string, std::string>> plugins;
						for (const auto &entry: std::filesystem::directory_iterator("plugin")) {
							const auto &path = entry.path();
							const std::string filename = "plugin/" + path.filename().string();
							if (!http.hasPlugin(path))
								plugins.emplace_back(escapeHTML(filename), escapeURL(filename));
						}
						return serve(http, client, RESOURCE(load, "load.t"), {
							{"css", RESOURCE(css, "style.css")},
							{"plugins", std::move(plugins)}});
					}
				} else if (parts.size() == 3) {
					if (parts[1] == "unload") {
						const auto &to_unload = parts[2];
						const std::string full_name = "plugin/" + to_unload;
						auto *tuple = http.getPlugin(full_name);
						if (tuple == nullptr)
							return serve(http, client, RESOURCE(error, "error.t"), {
								{"css", RESOURCE(css, "style.css")},
								{"error", "Plugin " + escapeHTML(to_unload) + " not loaded."}});
						http.unloadPlugin(*tuple);
						return serve(http, client, RESOURCE(unloaded, "unloaded.t"), {
							{"css", RESOURCE(css, "style.css")},
							{"plugin", escapeHTML(to_unload)}});
					}
					if (parts[1] == "load") {
						std::filesystem::path load_path = parts[2];
						if (http.hasPlugin(load_path))
							return serve(http, client, RESOURCE(error, "error.t"), {
								{"css", RESOURCE(css, "style.css")},
								{"error", "Plugin " + escapeHTML(parts[2]) + " already loaded."}});
						return serve(http, client, http.hasPlugin(load_path)? "true" : "false");
					}
				}

				http.server->send(client.id, HTTP::Response(404, "Invalid path").setMIME("text/plain"));
				http.server->close(client.id);
				return CancelableResult::Approve;
			} catch (const inja::RenderError &err) {
				ERROR(err.what());
			}
		}

		return CancelableResult::Pass;
	}

	CancelableResult Ansuz::serve(HTTP::Server &http, HTTP::Client &client, std::string_view content,
	                              const nlohmann::json &json, const char *mime) {
		if (json.empty())
			http.server->send(client.id, HTTP::Response(200, content).setMIME(mime));
		else
			http.server->send(client.id, HTTP::Response(200, inja::render(content, json)).setMIME(mime));
		http.server->close(client.id);
		return CancelableResult::Approve;
	}

	CancelableResult Ansuz::serveIndex(HTTP::Server &http, HTTP::Client &client) {
		const auto plugins = map(http.getPlugins(), [](const auto &tuple) {
			const auto &plugin = std::get<1>(tuple);
			return std::vector<std::string> {
				escapeHTML(std::string(std::filesystem::path(std::get<0>(tuple)).filename())),
				escapeHTML(plugin->getName()),
				escapeHTML(plugin->getDescription()),
				escapeHTML(plugin->getVersion()),
			};
		});

		nlohmann::json json {
			{"css", RESOURCE(css, "style.css")},
			{"plugins", plugins}
		};

		http.server->send(client.id, HTTP::Response(200, inja::render(RESOURCE(index, "index.t"),
			json)).setMIME("text/html"));
		http.server->close(client.id);
		return CancelableResult::Approve;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Ansuz;
}
