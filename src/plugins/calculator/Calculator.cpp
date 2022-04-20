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

		auto &[http, client, path, parts] = args;
		
		if (parts.size() == 3) {
			std::string_view oper = parts.front();
			long left, right;
			try {
				left = parseLong(std::string(parts.at(1)));
				right = parseLong(std::string(parts.at(2)));
			} catch (const std::invalid_argument &) {
				return CancelableResult::Pass;
			}

			std::string out;

			if (oper == "+" || oper == "plus")
				out = std::to_string(left + right);
			else if (oper == "-" || oper == "min")
				out = std::to_string(left - right);
			else if (oper == "*" || oper == "mult")
				out = std::to_string(left * right);
			else if (oper == "/" || oper == "div")
				out = right == 0? "Division by zero" : std::to_string(left / right);
			else
				return CancelableResult::Pass;

			http.server->send(client.id, HTTP::Response(200, out).setMIME("text/plain"));
			http.server->removeClient(client.id);
			return CancelableResult::Approve;
		}

		return CancelableResult::Pass;
	}
}

Algiz::Plugins::Calculator ext_plugin;
