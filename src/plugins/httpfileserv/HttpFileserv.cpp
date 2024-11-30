#include <inja/inja.hpp>

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

		http.getHandlers.emplace_back(getHandler);
		http.postHandlers.emplace_back(postHandler);

		if (auto iter = config.find("hosts"); iter != config.end()) {
			hostnames = iter->get<std::set<std::string>>();
		}

		if (auto iter = config.find("root"); iter != config.end()) {
			root = std::filesystem::canonical(iter->get<std::string>());
		}
	}

	void HttpFileserv::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).getHandlers, getHandler);
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).postHandlers, postHandler);
	}

	const std::filesystem::path & HttpFileserv::getRoot(const HTTP::Server &server) const {
		if (root) {
			return *root;
		}

		return server.webRoot;
	}

	bool HttpFileserv::hostMatches(const HTTP::Request &request) const {
		if (hostnames) {
			if (auto iter = request.headers.find("Host"); iter != request.headers.end()) {
				return hostnames->contains(iter->second);
			}
			return false;
		}

		return !request.headers.contains("Host");
	}

	static std::filesystem::path expand(const std::filesystem::path &web_root, std::string_view request_path) {
		return (std::filesystem::canonical(web_root) / ("./" + unescape(request_path, true))).lexically_normal();
	}

	CancelableResult HttpFileserv::handleGET(HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled) {
			return CancelableResult::Pass;
		}

		auto &[http, client, request, parts] = args;

		if (!hostMatches(request)) {
			return CancelableResult::Pass;
		}

		const std::filesystem::path &web_root = getRoot(http);

		auto full_path = expand(web_root, request.path);

		if (std::filesystem::is_directory(full_path)) {
			full_path /= "";
		}

		if (!isSubpath(web_root, full_path)) {
			ERROR("Not subpath of " << web_root << ": " << full_path);
			return CancelableResult::Pass;
		}

		if (authFailed(args, full_path)) {
			client.close();
			return CancelableResult::Kill;
		}

		auto nodot = http.getOption<bool>(full_path, "nodot");
		if (nodot && *nodot && full_path.filename().string()[0] == '.') {
			http.send401(client);
			client.close();
			return CancelableResult::Kill;
		}

		if (!findPath(full_path))
			return CancelableResult::Pass;

		try {
			const auto extension = full_path.extension();
			if (extension == ".t") {
				http.server->send(client.id,
					HTTP::Response(200, renderTemplate(readFile(full_path))).setMIME("text/html"));
			} else if (!request.ranges.empty() || request.suffixLength != 0) {
				serveRange(args, full_path);
			} else {
				serveFull(args, full_path);
			}
			client.close();
			return CancelableResult::Approve;
		} catch (const std::exception &err) {
			ERROR(err.what());
			client.send(HTTP::Response(403, "Forbidden"));
			client.close();
			return CancelableResult::Approve;
		}
	}

	CancelableResult HttpFileserv::handlePOST(HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		auto &[http, client, request, parts] = args;

		if (!hostMatches(request)) {
			return CancelableResult::Pass;
		}

		const std::filesystem::path &web_root = getRoot(http);

		auto full_path = expand(web_root, request.path);

		if (std::filesystem::is_directory(full_path)) {
			full_path /= "";
		}

		if (!isSubpath(web_root, full_path)) {
			ERROR("Not subpath of " << web_root << ": " << full_path);
			return CancelableResult::Pass;
		}

		if (authFailed(args, full_path)) {
			client.close();
			return CancelableResult::Kill;
		}

		if (http.getOption<bool>(full_path, "nodot") && full_path.filename().string()[0] == '.') {
			http.send401(client);
			client.close();
			return CancelableResult::Kill;
		}

		if (!findPath(full_path))
			return CancelableResult::Pass;

		try {
			const auto extension = full_path.extension();
			if (extension == ".t") {
				http.server->send(client.id,
					HTTP::Response(200, renderTemplate(readFile(full_path), {
						{"post", nlohmann::json(request.postParameters).dump()}
					})).setMIME("text/html"));
			} else if (!request.ranges.empty() || request.suffixLength != 0) {
				serveRange(args, full_path);
			} else {
				serveFull(args, full_path);
			}
			client.close();
			return CancelableResult::Approve;
		} catch (const std::exception &err) {
			ERROR(err.what());
			client.send(HTTP::Response(403, "Forbidden"));
			client.close();
			return CancelableResult::Approve;
		}
	}

	bool HttpFileserv::authFailed(HTTP::Server::HandlerArgs &args, const std::filesystem::path &full_path) const {
		auto &[http, client, request, parts] = args;

		auto auth = http.getOption(full_path, "auth");

		if (auth) {
			try {
				const std::string &username = auth->at("username");
				const std::string &password = auth->at("password");
				const auto auth_result = request.checkAuthentication(username, password);
				if (auth_result == HTTP::AuthenticationResult::Missing) {
					const std::string realm = auth->contains("realm")? auth->at("realm") : "";
					http.send401(client, realm);
					return true;
				}
				if (auth_result != HTTP::AuthenticationResult::Success) {
					http.send401(client);
					return true;
				}
			} catch (const nlohmann::detail::out_of_range &) {
				WARN("Invalid authentication configuration for " << full_path);
			}
		}

		if (full_path.filename() == ".algiz") {
			http.send401(client);
			return true;
		}

		return false;
	}

	bool HttpFileserv::findPath(std::filesystem::path &full_path) const {
		if (std::filesystem::is_regular_file(full_path)) {
			// Use the path as-is.
			return true;
		}

		for (const auto &default_filename: getDefaults()) {
			std::filesystem::path new_path = full_path / default_filename;

			if (std::filesystem::is_regular_file(new_path)) {
				full_path = std::move(new_path);
				return true;
			}
		}

		return false;
	}

	void HttpFileserv::serveRange(HTTP::Server::HandlerArgs &args, const std::filesystem::path &full_path) const {
		auto &[http, client, request, parts] = args;
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
	}

	void HttpFileserv::serveFull(HTTP::Server::HandlerArgs &args, const std::filesystem::path &full_path) const {
		auto &[http, client, request, parts] = args;
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

	std::vector<std::string> HttpFileserv::getDefaults() const {
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
