#include "Log.h"
#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/fileserv/Fileserv.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Shell.h"
#include "util/Templates.h"
#include "util/Usage.h"
#include "util/Util.h"

#include <dlfcn.h>
#include <inja/inja.hpp>

namespace {
	using FilterFunction = bool (*)(Algiz::HTTP::Server::HandlerArgs &, const std::filesystem::path &);

	constexpr std::string_view PREPROCESSED_EXTENSION = ".alg";

	void compileObject(std::filesystem::path source, const std::filesystem::path &output, bool preprocess, std::optional<std::string> &err_text) {
		using namespace Algiz;

		err_text.reset();

		if (preprocess) {
			char temp_late[]{"/tmp/algiz-pp-XXXXXX.cpp"};
			int fd = mkstemps(temp_late, sizeof(".cpp") - 1);
			if (fd == -1) {
				throw std::runtime_error("Failed to make a temporary file for module preprocessing");
			}
			try {
				std::string preprocessed = Algiz::Plugins::preprocessFileservModule(source);
				if (write(fd, preprocessed.data(), preprocessed.size()) == -1) {
					throw std::runtime_error("Couldn't write preprocessed module");
				}
				source = temp_late;
			} catch (...) {
				close(fd);
				throw;
			}
			close(fd);
		}

		CommandOutput result = runCommand("c++", {"-std=c++23", "-Iinclude", "-Isubprojects/wahtwo/include", "-fPIC", source.string(), "-shared", "-o", output.string()});
		if (result.code || result.signal != -1) {
			if (!result.err.empty()) {
				ERROR("Failed to compile " << source << ":\n" << result.err);
				err_text = std::move(result.err);
			}
			throw std::runtime_error(std::format("Failed to compile {} to {}", source.string(), output.string()));
		} else if (preprocess) {
			std::filesystem::remove(source);
		}
	}

	FilterFunction getFilterFunction(const char *name) {
		return reinterpret_cast<FilterFunction>(dlsym(RTLD_DEFAULT, name));
	}

	bool doFilter(Algiz::HTTP::Server::HandlerArgs &args, const std::filesystem::path &path, const char *function_name, auto &&default_action) {
		if (auto function = getFilterFunction(function_name)) {
			return function(args, path);
		}

		if constexpr (std::invocable<decltype(default_action), decltype(args), decltype(path)>) {
			return default_action(args, path);
		} else {
			return default_action();
		}
	}
}

namespace Algiz::Plugins {
	Fileserv::Fileserv():
		moduleCache(64),
		rng(std::random_device{}()) {}

	void Fileserv::postinit(PluginHost *host) {
		auto &http = dynamic_cast<HTTP::Server &>(*(parent = host));

		http.getHandlers.emplace_back(getHandler);
		http.postHandlers.emplace_back(postHandler);

		if (auto iter = config.find("hosts"); iter != config.end()) {
			hostnames = iter->get<std::set<std::string>>();
		}

		if (auto iter = config.find("root"); iter != config.end()) {
			root = std::filesystem::canonical(iter->get<std::string>());
		}

		if (auto iter = config.find("enableModules"); iter != config.end()) {
			enableModules = *iter;
		}
	}

	void Fileserv::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).getHandlers, getHandler);
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).postHandlers, postHandler);
	}

	const std::filesystem::path & Fileserv::getRoot(const HTTP::Server &server) const {
		if (root) {
			return *root;
		}

		return server.webRoot;
	}

	bool Fileserv::hostMatches(const HTTP::Request &request) const {
		if (hostnames) {
			if (auto iter = request.headers.find("host"); iter != request.headers.end()) {
				return hostnames->contains(iter->second);
			}
			return false;
		}

		return !request.headers.contains("host");
	}

	static std::filesystem::path expand(const std::filesystem::path &web_root, std::string_view request_path) {
		return (std::filesystem::canonical(web_root) / ("./" + unescape(request_path, true))).lexically_normal();
	}

	CancelableResult Fileserv::handleGET(HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled) {
			return CancelableResult::Pass;
		}

		auto &[http, client, request, parts] = args;

		if (!hostMatches(request)) {
			return CancelableResult::Pass;
		}

		const std::filesystem::path &web_root = getRoot(http);

		std::filesystem::path full_path = expand(web_root, request.path);

		if (std::filesystem::is_directory(full_path)) {
			full_path /= "";
		}

		if (!isSubpath(web_root, full_path)) {
			ERROR("Not subpath of " << web_root << ": " << full_path);
			return CancelableResult::Pass;
		}

		if (!filter(args, full_path)) {
			client.close();
			return CancelableResult::Kill;
		}

		auto nodot = http.getOption<bool>(full_path, "nodot");
		if (nodot && *nodot && full_path.filename().string()[0] == '.') {
			http.send401(client);
			client.close();
			return CancelableResult::Kill;
		}

		if (!findPath(full_path)) {
			return CancelableResult::Pass;
		}

		try {
			const auto extension = full_path.extension();

			if (extension == ".t") {
				http.server->send(client.id, HTTP::Response(200, renderTemplate(readFile(full_path))).setMIME("text/html"));
			} else if (shouldServeModule(http, full_path)) {
				serveModule(args, full_path);
			} else if (!request.hackRanges() && (!request.ranges.empty() || request.suffixLength != 0)) {
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

	CancelableResult Fileserv::handlePOST(HTTP::Server::HandlerArgs &args, bool not_disabled) {
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

		if (!filter(args, full_path)) {
			client.close();
			return CancelableResult::Kill;
		}

		if (http.getOption<bool>(full_path, "nodot") && full_path.filename().string()[0] == '.') {
			http.send401(client);
			client.close();
			return CancelableResult::Kill;
		}

		if (!findPath(full_path)) {
			return CancelableResult::Pass;
		}

		try {
			const auto extension = full_path.extension();
			if (extension == ".t") {
				http.server->send(client.id, HTTP::Response(200, renderTemplate(readFile(full_path), {
					{"post", nlohmann::json(request.postParameters).dump()}
				})).setMIME("text/html"));
			} else if (shouldServeModule(http, full_path)) {
				serveModule(args, full_path);
			} else if (!request.hackRanges() && (!request.ranges.empty() || request.suffixLength != 0)) {
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

	bool Fileserv::authFailed(HTTP::Server::HandlerArgs &args, const std::filesystem::path &full_path) const {
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

	bool Fileserv::findPath(std::filesystem::path &full_path) const {
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

	void Fileserv::serveRange(HTTP::Server::HandlerArgs &args, const std::filesystem::path &full_path) const {
		auto &[http, client, request, parts] = args;
		const size_t filesize = std::filesystem::file_size(full_path);
		if (!request.valid(filesize)) {
			http.send400(client);
			return;
		}

		std::ifstream stream;
		stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		stream.open(full_path);
		stream.exceptions(std::ifstream::goodbit);
		if (!stream.is_open()) {
			http.send403(client);
			return;
		}

		HTTP::Response response(206, "");

		size_t length = request.suffixLength;

		for (const auto &[start, end]: request.ranges) {
			length += end - start + 1;
		}

		response.setAcceptRanges();
		response["content-length"] = std::to_string(length);
		response.setLastModified(lastWritten(full_path));

		char boundary_bytes[]{"--________________"};
		std::string_view boundary{boundary_bytes, sizeof(boundary_bytes) - 1};

		assert(!request.ranges.empty());
		const bool multi = request.ranges.size() > 1;

		std::string &content_type = response["content-type"];
		const std::string old_content_type = content_type;

		if (multi) {
			std::uniform_int_distribution<char> distribution('a', 'z');
			for (size_t i = 2; i < boundary.size(); ++i) {
				boundary_bytes[i] = distribution(rng);
			}
			content_type += std::format("; boundary={}", boundary.substr(2));
		 } else {
			auto [start, end] = request.ranges[0];
			response["content-range"] = std::format("bytes {}-{}/{}", start, end, filesize);
		 }

		auto send_start = [&](size_t start, size_t end) {
			if (multi) {
				http.server->send(client.id, std::format("{}\r\nContent-Type: {}\r\nContent-Range: {}-{}/{}\r\n\r\n", boundary, old_content_type, start, end, filesize));
			}
		};

		http.server->send(client.id, response.noContent());

		std::unique_ptr<char[]> buffer;
		size_t old_buffer_length = 0;

		for (const auto &[start, end]: request.ranges) {
			send_start(start, end);

			size_t remaining = end - start + 1;
			stream.seekg(start, std::ios::beg);

			if (size_t buffer_length = std::min(chunkSize, remaining); !buffer || old_buffer_length != buffer_length) {
				buffer = std::make_unique<char[]>(buffer_length);
				old_buffer_length = buffer_length;
			}

			while (0 < remaining) {
				const size_t to_read = std::min(chunkSize, remaining);
				stream.read(buffer.get(), to_read);
				http.server->send(client.id, std::string_view(buffer.get(), to_read));
				remaining -= to_read;
			}
		}

		if (request.suffixLength != 0) {
			send_start(filesize - request.suffixLength, filesize - 1);

			size_t remaining = request.suffixLength;
			stream.seekg(filesize - request.suffixLength, std::ios::beg);

			buffer = std::make_unique<char[]>(std::min(chunkSize, remaining));

			while (0 < remaining) {
				const size_t to_read = std::min(chunkSize, remaining);
				stream.read(buffer.get(), to_read);
				http.server->send(client.id, std::string_view(buffer.get(), to_read));
				remaining -= to_read;
			}
		}

		if (multi) {
			http.server->send(client.id, boundary);
			http.server->send(client.id, std::string_view{"--"});
		}
	}

	void Fileserv::serveFull(HTTP::Server::HandlerArgs &args, const std::filesystem::path &full_path) const {
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
			response["content-length"] = std::to_string(filesize);
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

	void Fileserv::serveModule(HTTP::Server::HandlerArgs &args, const std::filesystem::path &full_path) const {
		std::filesystem::path object = full_path;
		object.replace_extension(object.extension().string() + ".so");
		std::optional<std::string> err_text;
		if (!std::filesystem::exists(object) || isNewerThan(full_path, object)) {
			moduleCache.remove(object);
			try {
				compileObject(full_path, object, full_path.extension() == PREPROCESSED_EXTENSION, err_text);
			} catch (...) {
				args.server.server->send(args.client.id, HTTP::Response(500, err_text.value_or("Compilation failed"), "text/plain").setCharset("utf-8"));
				return;
			}
		}

		auto [function, lock] = moduleCache[object];
		function(args);
	}

	std::vector<std::string> Fileserv::getDefaults() const {
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

	bool Fileserv::shouldServeModule(HTTP::Server &http, const std::filesystem::path &path) const {
		if (!getModulesEnabled(http, path)) {
			return false;
		}
		std::filesystem::path extension = path.extension();
		return (extension == ".cpp" && canExecute(path)) || extension == PREPROCESSED_EXTENSION;
	}

	bool Fileserv::getModulesEnabled(HTTP::Server &http, const std::filesystem::path &path) const {
		if (enableModules) {
			return true;
		}

		auto option = http.getOption<bool>(path, "enableModules");
		return option && *option;
	}

	bool Fileserv::filter(HTTP::Server::HandlerArgs &args, const std::filesystem::path &path) const {
		auto &[http, client, request, parts] = args;

		if (!doFilter(args, path, "algizAuthCheck", [&] { return !authFailed(args, path); })) {
			return doFilter(args, path, "algizAuthCheckFailed", [&] {
				http.server->send(client.id, HTTP::Response(401, "Unauthorized"));
				return false;
			});
		}

		// Executable files ending in ".alg.so" or ".cpp.so" must not be served if modules are enabled.
		auto module_filter = [&] {
			if (getModulesEnabled(http, path) && path.extension() == ".so" && canExecute(path)) {
				std::filesystem::path subextension = path.stem().extension();
				if (subextension == ".alg" || subextension == ".cpp") {
					return false;
				}
			}

			return true;
		};

		if (!doFilter(args, path, "algizCompiledModuleCheck", std::move(module_filter))) {
			return doFilter(args, path, "algizCompiledModuleCheckFailed", [&] {
				http.server->send(client.id, HTTP::Response(401, "Nice try."));
				return false;
			});
		}

		return true;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Fileserv;
}
