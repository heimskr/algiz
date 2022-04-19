#include "plugins/HttpFileserv.h"

namespace Algiz::Plugins {
	Plugins::CancelableResult HttpFileserv::handle(const HTTP::Server::HandlerArgs &args, bool not_disabled) {
		auto &[server, client, path] = args;
		return Plugins::CancelableResult::Approve;
	}
}
