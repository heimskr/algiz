#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/Calculator.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Util.h"
#include "Log.h"

namespace Algiz::Plugins {
	void Calculator::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).handlers.push_back(handler);
	}

	void Calculator::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).handlers, std::weak_ptr(handler));
	}

	CancelableResult Calculator::handle(const HTTP::Server::HandlerArgs &args, bool non_disabled) {
		if (!non_disabled)
			return CancelableResult::Pass;

		auto &[server, client, path] = args;
		auto pieces = split(std::string_view(path).substr(1), "/", true);
		
		if (pieces.size() == 3) {
			std::string_view oper = pieces.front();
			long left, right;
			try {
				left = parseLong(std::string(pieces.at(1)));
				right = parseLong(std::string(pieces.at(2)));
			} catch (const std::invalid_argument &) {
				return CancelableResult::Pass;
			}

			std::string out;

			if (oper == "+") {
				out = std::to_string(left + right);
			} else if (oper == "-") {
				out = std::to_string(left - right);
			} else if (oper == "*") {
				out = std::to_string(left * right);
			} else if (oper == "/") {
				out = std::to_string(left / right);
			}

			server.send(client.id, HTTP::Response(200, out).setMIME("text/plain"), true);
			server.removeClient(client.id);
			return CancelableResult::Approve;
		}

		return CancelableResult::Pass;
	}
}

Algiz::Plugins::Calculator ext_plugin;
