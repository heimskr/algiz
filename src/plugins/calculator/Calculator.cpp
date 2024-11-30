#include "Log.h"
#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/Calculator.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Util.h"

namespace Algiz::Plugins {
	void Calculator::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).getHandlers.push_back(handler);
	}

	void Calculator::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).getHandlers, handler);
	}

	CancelableResult Calculator::handle(const HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		const auto &[http, client, request, parts] = args;

		if (parts.size() == 3) {
			std::string_view oper = parts.front();
			int64_t left = 0;
			int64_t right = 0;
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
			http.server->close(client.id);
			return CancelableResult::Approve;
		}

		return CancelableResult::Pass;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Calculator;
}
