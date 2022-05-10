#include <inja.hpp>

#include "Log.h"
#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/HttpFileserv.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Templates.h"
#include "util/Usage.h"
#include "util/Util.h"

namespace Algiz::Plugins {
	void HttpFileserv::postinit(PluginHost *host) {
		auto &http = dynamic_cast<HTTP::Server &>(*(parent = host));
		http.handlers.push_back(std::weak_ptr(handler));
	}

	void HttpFileserv::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).handlers, std::weak_ptr(handler));
	}

	CancelableResult HttpFileserv::handle(HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		auto &[http, client, request, parts] = args;
		auto full_path = (http.webRoot / ("./" + unescape(request.path, true))).lexically_normal();
		if (std::filesystem::is_directory(full_path))
			full_path /= "";

		if (!isSubpath(http.webRoot, full_path)) {
			ERROR("Not subpath of " << http.webRoot << ": " << full_path);
			return CancelableResult::Pass;
		}

		auto dir = full_path.parent_path();
		const auto root = dir.root_path();
		do {
			auto auth = http.getOption(dir, "auth");
			if (auth) {
				try {
					const std::string &username = auth->at("username");
					const std::string &password = auth->at("password");
					const auto auth_result = request.checkAuthentication(username, password);
					if (auth_result == HTTP::AuthenticationResult::Missing) {
						const std::string realm = auth->contains("realm")? auth->at("realm") : "";
						http.send401(client, realm);
						return CancelableResult::Approve;
					}
					if (auth_result != HTTP::AuthenticationResult::Success) {
						http.send401(client);
						return CancelableResult::Approve;
					}
					break;
				} catch (const nlohmann::detail::out_of_range &) {
					WARN("Invalid authentication configuration in " << dir);
				}
			}
			if (dir == http.webRoot)
				break;
			dir = dir.parent_path();
		} while (dir != root);

		if (full_path.filename() == ".algiz") {
			http.send401(client);
			return CancelableResult::Approve;
		}

		if (std::filesystem::is_regular_file(full_path)) {
			// Use the path as-is.
		} else {
			bool found = false;
			for (const auto &default_filename: getDefaults()) {
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
			const auto extension = full_path.extension();
			if (extension == ".t") {
				http.server->send(client.id,
					HTTP::Response(200, renderTemplate(readFile(full_path))).setMIME("text/html"));
			} else if (!request.ranges.empty() || request.suffixLength != 0) {
				const size_t filesize = std::filesystem::file_size(full_path);
				if (request.valid(filesize)) {
					std::ifstream stream;
					stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
					stream.open(full_path);
					stream.exceptions(std::ifstream::goodbit);
					if (!stream.is_open()) {
						http.send403(client);
					} else {
						HTTP::Response response(206, "");

						size_t length = request.suffixLength;
						for (const auto &[start, end]: request.ranges)
							length += end - start + 1;

						response.setAcceptRanges();
						response["Content-Length"] = std::to_string(length);
						response.setLastModified(lastWritten(full_path));

						http.server->send(client.id, response.noContent());
						for (const auto &[start, end]: request.ranges) {
							size_t remaining = end - start + 1;
							stream.seekg(start, std::ios::beg);
							auto buffer = std::make_unique<char[]>(std::min(chunkSize, remaining));
							while (0 < remaining) {
								const size_t to_read = std::min(chunkSize, remaining);
								stream.read(buffer.get(), to_read);
								http.server->send(client.id, std::string_view(buffer.get(), to_read));
								remaining -= to_read;
							}
						}

						if (request.suffixLength != 0) {
							size_t remaining = request.suffixLength;
							stream.seekg(filesize - request.suffixLength, std::ios::beg);
							auto buffer = std::make_unique<char[]>(std::min(chunkSize, remaining));
							while (0 < remaining) {
								const size_t to_read = std::min(chunkSize, remaining);
								stream.read(buffer.get(), to_read);
								http.server->send(client.id, std::string_view(buffer.get(), to_read));
								remaining -= to_read;
							}
						}
					}
				} else
					http.send400(client);
			} else {
				std::ifstream stream;
				stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
				stream.open(full_path);
				stream.exceptions(std::ifstream::goodbit);
				if (!stream.is_open()) {
					http.send403(client);
				} else {
					const std::string mime = getMIME(full_path.extension());
					const size_t filesize = std::filesystem::file_size(full_path);
					HTTP::Response response(200, "");
					response.setLastModified(lastWritten(full_path)).setAcceptRanges().setMIME(mime);
					response["Content-Length"] = std::to_string(filesize);
					http.server->send(client.id, response.noContent());
					size_t remaining = filesize;
					stream.seekg(0, std::ios::beg);
					auto buffer = std::make_unique<char[]>(std::min(chunkSize, remaining));
					while (0 < remaining) {
						const size_t to_read = std::min(chunkSize, remaining);
						stream.read(buffer.get(), to_read);
						http.server->send(client.id, std::string_view(buffer.get(), to_read));
						remaining -= to_read;
					}
				}
			}
			client.close();
			return CancelableResult::Approve;
		} catch (std::exception &err) {
			ERROR(err.what());
			client.send(HTTP::Response(403, "Forbidden"));
			client.close();
			return CancelableResult::Approve;
		}
	}

	std::vector<std::string> HttpFileserv::getDefaults() {
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

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::HttpFileserv;
}
