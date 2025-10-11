#include "util/Util.h"
#include "plugins/fileserv/Fileserv.h"

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
			DiagnosticConsumer(PreprocessAction &action):
				action(action) {}

		private:
			PreprocessAction &action;

			void HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic &diagnostic) override;
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
				diagnostics.setClient(new DiagnosticConsumer(*this), true);
				clang::ASTFrontendAction::ExecuteAction();
			}

		private:
			bool BeginSourceFileAction(clang::CompilerInstance &) override {
				return true;
			}
	};

	std::string preprocessFileservModule(const std::filesystem::path &path) {
		std::vector<std::string> sources{path.string()};
		std::array argv{"algiz", "--", path.c_str(), static_cast<const char *>(nullptr)};
		int argc(argv.size() - 1);
		llvm::cl::OptionCategory category("algiz-preprocess options");
		auto expected = clang::tooling::CommonOptionsParser::create(argc, argv.data(), category, llvm::cl::ZeroOrMore);
		if (!expected) {
			throw std::runtime_error("Couldn't construct CommonOptionsParser");
		}
		clang::tooling::ClangTool tool(expected->getCompilations(), sources);
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
			if (use_preamble) {
				view.remove_prefix(1);
			}

			size_t last = findLast(view, CODE_END);
			if (last < std::string::npos) {
				last += CODE_END.size();
			}
			std::string_view shortened = view.substr(0, last);

			auto action = std::make_unique<PreprocessAction>(shortened, delimiter_end);
			clang::tooling::runToolOnCode(std::move(action), llvm::Twine(shortened));

			if (!delimiter_end) {
				throw std::runtime_error("Syntax error");
			}

			size_t valid_size = *delimiter_end;
			std::stringstream &stream = use_preamble? preamble : body;
			stream << shortened.substr(0, valid_size) << '\n';
			view.remove_prefix(valid_size + CODE_END.size());
		}

		preamble << R"(
#include "Module.h"

#include <sstream>

extern "C" void algizModule(HTTP::Server::HandlerArgs &algiz_args) {
	auto &[http, client, request, parts] = algiz_args;

	int code = 200;

	std::stringstream stream;
	auto echo = [&](const auto &thing) {
		stream << thing;
	};

		)" << body.view() << R"(

	http.server->send(client.id, HTTP::Response(code, stream.view()));
}
		)";

		return std::move(preamble).str();
	}

	void DiagnosticConsumer::HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic &diagnostic) {
		llvm::SmallVector<char> message_vector;
		diagnostic.FormatDiagnostic(message_vector);
		std::string_view message(message_vector.data(), message_vector.size());
		const clang::DiagnosticsEngine *diagnostics = diagnostic.getDiags();
		assert(diagnostics != nullptr);
		auto ids = diagnostics->getDiagnosticIDs();
		unsigned category = ids->getCategoryNumberForDiag(diagnostic.getID());

		if (category == clang::diag::DiagCat_Lexical_or_Preprocessor_Issue && message.find("file not found") != std::string::npos) {
			// don't really care
			return;
		}

		if (level >= clang::DiagnosticsEngine::Level::Error || message == "extra tokens at end of #include directive") {
			if (category == clang::diag::DiagCat_Parse_Issue || category == clang::diag::DiagCat_Lexical_or_Preprocessor_Issue) {
				clang::SourceManager &manager = action.getCompilerInstance().getSourceManager();
				const clang::SourceLocation &location = diagnostic.getLocation();
				const unsigned offset = manager.getFileOffset(location);
				// We need enough space for the end delimiter.
				if (offset + CODE_END.size() <= action.code.size()) {
					if (action.code.substr(offset, CODE_END.size()) == CODE_END) {
						if (!action.delimiterEnd || offset < *action.delimiterEnd) {
							action.delimiterEnd = offset;
						}
					}
				}
			}
		}
	}
}
