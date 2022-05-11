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

// #define EXTERNAL_RESOURCES

#ifdef EXTERNAL_RESOURCES
#define RESOURCE(a, b) readFile("res/ansuz/" b)
#else
#define RESOURCE(a, b) std::string_view(ansuz_##a, ansuz_##a##_len)
#endif

#define CSS {"css", RESOURCE(css, "style.css")}
#define MESSAGE RESOURCE(message, "message.t")

namespace Algiz::Plugins {
	void Ansuz::postinit(PluginHost *host) {
		auto &http = dynamic_cast<HTTP::Server &>(*(parent = host));
		http.getHandlers.push_back(getHandler);
		http.postHandlers.push_back(postHandler);
	}

	void Ansuz::cleanup(PluginHost *host) {
		auto &http = dynamic_cast<HTTP::Server &>(*host);
		PluginHost::erase(http.getHandlers, std::weak_ptr(getHandler));
		PluginHost::erase(http.postHandlers, std::weak_ptr(postHandler));
	}

	CancelableResult Ansuz::handleGET(HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		auto &[http, client, request, parts] = args;

		if (!parts.empty() && parts.front() == "ansuz") {
			if (!checkAuth(http, client, request))
				return CancelableResult::Kill;

			try {
				if (parts.size() == 1)
					return serveIndex(http, client);

				if (parts.size() == 2) {
					if (parts[1] == "load") {
						std::vector<std::pair<std::string, std::string>> plugins;
						for (const auto &entry: std::filesystem::directory_iterator("plugin")) {
							const auto &path = entry.path();
							const std::string filename = "plugin/" + path.filename().string();
							if (!http.hasPlugin(path))
								plugins.emplace_back(escapeHTML(filename), escapeURL(filename));
						}
						return serve(http, client, RESOURCE(load, "load.t"), {CSS,
							{"plugins", std::move(plugins)}});
					}
				} else if (parts.size() == 3) {
					if (parts[1] == "unload") {
						const auto &to_unload = parts[2];
						const auto canonical = std::filesystem::canonical("plugin/" + to_unload);
						auto *tuple = http.getPlugin(canonical);
						if (tuple == nullptr)
							return serve(http, client, MESSAGE, {CSS,
								{"message", "Plugin " + escapeHTML(to_unload) + " not loaded."}});
						http.unloadPlugin(*tuple);
						return serve(http, client, RESOURCE(unloaded, "unloaded.t"), {CSS,
							{"plugin", escapeHTML(to_unload)}});
					} else if (parts[1] == "edit") {
						const auto &to_edit = parts[2];
						const auto canonical = std::filesystem::canonical("plugin/" + to_edit);
						auto *tuple = http.getPlugin(canonical);
						if (tuple == nullptr)
							return serve(http, client, MESSAGE, {CSS,
								{"message", "Plugin " + escapeHTML(to_edit) + " not loaded."}});
						return serve(http, client, RESOURCE(edit_config, "edit_config.t"), {CSS,
							{"config", std::get<1>(*tuple)->getConfig().dump()},
							{"pluginName", escapeHTML(escapeQuotes(canonical.string()))}
						});
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

	CancelableResult Ansuz::handlePOST(HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		auto &[http, client, request, parts] = args;

		if (2 <= parts.size() && parts.front() == "ansuz") {
			if (!checkAuth(http, client, request))
				return CancelableResult::Kill;

			try {
				if (parts[1] == "load") {
					const auto &post = request.postParameters;
					if (!post.contains("pluginName") || !post.contains("pluginConfig"))
						return serve(http, client, MESSAGE, {CSS, {"message", "Invalid request."}}, 400);
					const auto &name = post.at("pluginName");
					std::filesystem::path path(name);
					if (http.hasPlugin(path))
						return serve(http, client, MESSAGE, {CSS, {"message", "Plugin already loaded."}}, 404);
					const auto json = nlohmann::json::parse(post.at("pluginConfig"));
					auto result = http.loadPlugin(path);
					auto &plugin = std::get<1>(result);
					plugin->setConfig(std::move(json));
					plugin->postinit(&http);
					return serve(http, client, MESSAGE, {CSS,
						{"message", "Plugin loaded. <pre>" + escapeHTML(json.dump()) + "</pre>"}});
				} else if (parts[1] == "edit") {
					const auto &post = request.postParameters;
					if (!post.contains("pluginName") || !post.contains("pluginConfig"))
						return serve(http, client, MESSAGE, {CSS, {"message", "Invalid request."}}, 400);
					const auto json = nlohmann::json::parse(post.at("pluginConfig"));
					const auto &name = post.at("pluginName");
					auto path = std::filesystem::canonical(name);
					if (!http.hasPlugin(path))
						return serve(http, client, MESSAGE, {CSS, {"message", "Plugin not loaded."}}, 404);
					auto *tuple = http.getPlugin(path);
					if (!tuple)
						return serve(http, client, MESSAGE, {CSS, {"message", "Couldn't get plugin."}}, 500);
					std::get<1>(*tuple)->setConfig(json);
					return serve(http, client, MESSAGE, {CSS, {"message", "Configuration updated."}});
				}
			} catch (const std::exception &err) {
				ERROR(err.what());
				return serve(http, client, MESSAGE, {CSS, {"message", escapeHTML(err.what())}}, 500);
			}
		}

		return CancelableResult::Pass;
	}

	bool Ansuz::checkAuth(HTTP::Server &server, HTTP::Client &client, const HTTP::Request &request) {
		if (!config.contains("password") || !config.at("password").is_string()) {
			serve(server, client, MESSAGE, {CSS, {"message", "Ansuz needs a password in its configuration."}});
			return false;
		}

		const auto auth = request.checkAuthentication("ansuz", config.at("password").get<std::string>());
		if (auth == HTTP::AuthenticationResult::Missing) {
			server.send401(client, "Ansuz");
			return false;
		}

		if (auth != HTTP::AuthenticationResult::Success) {
			serve(server, client, MESSAGE, {CSS, {"message", "Bad authentication."}}, 401);
			return false;
		}

		return true;
	}

	CancelableResult Ansuz::serve(HTTP::Server &http, HTTP::Client &client, std::string_view content,
	                              const nlohmann::json &json, int code, const char *mime) {
		if (json.empty())
			http.server->send(client.id, HTTP::Response(code, content).setMIME(mime));
		else
			http.server->send(client.id, HTTP::Response(code, inja::render(content, json)).setMIME(mime));
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

		http.server->send(client.id, HTTP::Response(200, inja::render(RESOURCE(index, "index.t"), json)));
		http.server->close(client.id);
		return CancelableResult::Approve;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Ansuz;
}
