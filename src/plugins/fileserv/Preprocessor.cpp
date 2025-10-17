#include "plugins/fileserv/Fileserv.h"
#include "util/Util.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

#include <array>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace {
	constexpr std::string_view CODE_START = "<?";
	constexpr std::string_view CODE_END = "?>";
}

namespace Algiz::Plugins {
	class PreprocessAction;

	class DiagnosticConsumer final: public clang::DiagnosticConsumer {
		public:
			DiagnosticConsumer() = default;

		private:
			void HandleDiagnostic(clang::DiagnosticsEngine::Level, const clang::Diagnostic &) override {
				// Swallow warnings/errors
			}
	};

	class PreprocessAction final: public clang::ASTFrontendAction {
		friend class DiagnosticConsumer;

		public:
			PreprocessAction(std::string_view code, std::optional<size_t> &delimiterEnd):
				code(code),
				delimiterEnd(delimiterEnd) {}

		protected:
			std::string_view code;
			std::optional<size_t> &delimiterEnd;

			std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &, llvm::StringRef) override {
				return std::make_unique<clang::ASTConsumer>();
			}

			void ExecuteAction() override {
				clang::DiagnosticsEngine &diagnostics = getCompilerInstance().getDiagnostics();
				diagnostics.setClient(new DiagnosticConsumer, true);
				clang::ASTFrontendAction::ExecuteAction();
			}
	};

	std::optional<size_t> findCodeEnd(std::string_view source) {
		clang::LangOptions lang_options;
		clang::CommentOptions::BlockCommandNamesTy includes;
		clang::LangOptions::setLangDefaults(lang_options, clang::Language::CXX, llvm::Triple{}, includes, clang::LangStandard::lang_cxx26);
		clang::Lexer lexer(clang::SourceLocation(), lang_options, source.begin(), source.begin(), source.end());
		for (clang::Token previous_token{}, token{}; !lexer.LexFromRawLexer(token) && !token.is(clang::tok::eof); previous_token = token) {
			if (previous_token.is(clang::tok::question)) {
				if (token.is(clang::tok::greater) || token.is(clang::tok::greatergreater) || token.is(clang::tok::greaterequal) || token.is(clang::tok::greatergreaterequal) || token.is(clang::tok::greatergreatergreater)) {
					ssize_t offset = lexer.getCurrentBufferOffset();
					offset -= token.getLength() + 1;
					assert(offset >= 0);
					if (offset >= std::ssize(source)) {
						break;
					}

					if (source.at(offset) == '?') {
						return offset;
					}
				}
			}
		}

		return std::nullopt;
	}

	std::string preprocessFileservModule(const std::filesystem::path &path) {
		std::string source = readFile(path);
		std::string_view view(source);
		std::stringstream preamble;
		std::stringstream body;

		for (;;) {
			std::optional<size_t> delimiter_end;
			size_t code_start = view.find(CODE_START);

			if (code_start == std::string::npos) {
				if (!view.empty()) {
					body << "echo(\"" << escape(view) << "\");\n";
				}

				break;
			}

			if (code_start != 0) {
				body << "echo(\"" << escape(view.substr(0, code_start)) << "\");\n";
			}

			view.remove_prefix(code_start + CODE_START.size());

			const bool use_preamble = !view.empty() && view[0] == ':';
			const bool echo_shorthand = !view.empty() && view[0] == '=';

			if (use_preamble || echo_shorthand) {
				view.remove_prefix(1);
			}

			std::string_view shortened = view.substr(0, findLast(view, CODE_END));
			std::string surrounded;
			std::unique_ptr<PreprocessAction> action;

			if (std::optional<size_t> offset = findCodeEnd(shortened)) {
				delimiter_end = *offset;
				if (echo_shorthand) {
					surrounded = R"(
#include "include/Module.h"

extern "C" void algizModule(HTTP::Server::HandlerArgs &algiz_args, const std::filesystem::path &path) {
	auto &[$http, $client, $request, $parts] = algiz_args;
	auto &$get = $request.parameters;
	auto &$post = $request.postParameters;
	int $code = 200;
	std::stringstream $stream;

	auto echo = [](auto &&...) {};

	echo(
					)";
					surrounded += shortened.substr(0, *offset);
					surrounded += R"(
	);
	// */
}
					)";
					action = std::make_unique<PreprocessAction>(surrounded, delimiter_end);
				}
			}

			if (!action) {
				action = std::make_unique<PreprocessAction>(shortened, delimiter_end);
			}

			std::stringstream &stream = use_preamble? preamble : body;
			clang::tooling::runToolOnCode(std::move(action), llvm::Twine(shortened));

			if (!delimiter_end) {
				delimiter_end = shortened.size();
			}

			size_t valid_size = *delimiter_end;
			std::string_view valid = shortened.substr(0, valid_size);
			if (echo_shorthand) {
				stream << "echo(" << valid << ");\n";
			} else {
				stream << valid << '\n';
			}
			view.remove_prefix(valid_size + CODE_END.size());
		}

		preamble << R"(
#include "Module.h"

#include <sstream>

extern "C" void algizModule(HTTP::Server::HandlerArgs &algiz_args, const std::filesystem::path &$path) {
	auto &[$http, $client, $request, $parts] = algiz_args;
	auto &$get = $request.parameters;
	auto &$post = $request.postParameters;
	int $code = 200;
	std::stringstream $stream;

	auto echo = [&](auto &&...things) {
		($stream << ... << std::forward<decltype(things)>(things));
	};

		)" << body.view() << R"(

	$http.server->send($client.id, HTTP::Response($code, $stream.view()));
	// */
}
		)";

		return std::move(preamble).str();
	}
}
