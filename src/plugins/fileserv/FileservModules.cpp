#include "util/Util.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "plugins/fileserv/Fileserv.h"

#include <array>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace {
	constexpr std::string_view CODE_START = "<?";
	constexpr std::string_view CODE_END = "?>";
	constexpr std::string_view RAW_START = "<%";
	constexpr std::string_view RAW_END = "%>";
}

namespace Algiz::Plugins {
	enum class Delimiter {Code, Raw};

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
			PreprocessAction(std::string_view code, std::optional<size_t> &delimiterEnd, Delimiter delimiter):
				code(code),
				delimiterEnd(delimiterEnd),
				delimiter(delimiter) {}

		protected:
			std::string_view code;
			std::optional<size_t> &delimiterEnd;
			Delimiter delimiter;

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

	std::string preprocessModule(const std::filesystem::path &path) {
		char tempdir_template[]{"/tmp/algiz-pp-XXXXXX"};
		if (mkdtemp(tempdir_template) == nullptr) {
			throw std::runtime_error("Couldn't create temporary directory for module preprocessing");
		}
		std::filesystem::path tempdir{tempdir_template};
		std::vector<std::string> sources{path.string()};
		std::array argv{"algiz", "--", "-I/usr/include/c++/v1", path.c_str(), static_cast<const char *>(nullptr)};
		int argc(argv.size() - 1);
		llvm::cl::OptionCategory category("algiz-preprocess options");
		auto expected = clang::tooling::CommonOptionsParser::create(argc, argv.data(), category, llvm::cl::ZeroOrMore);
		if (!expected) {
			throw std::runtime_error("Couldn't construct CommonOptionsParser");
		}
		clang::tooling::ClangTool tool(expected->getCompilations(), sources);
		std::string output = R"(
extern "C" void algizModule(HTTP::Server::HandlerArgs &args) {
	auto &[http, client, request, parts] = args;
	http.server->send(client.id, HTTP::Response(200, "preprocessing not yet implemented"));
}
		)";
		std::string source = readFile(path);
		std::string_view view(source);
		std::filesystem::path build_path = tempdir / "input.alg";
		std::stringstream build_stream(build_path);

		for (;;) {
			std::optional<size_t> delimiter_end;
			size_t code_start = view.find(CODE_START);
			size_t raw_start = view.find(RAW_START);

			if (code_start < raw_start) {
				if (code_start != 0) {
					build_stream << "echo(\"" << escape(view.substr(0, code_start)) << "\");\n";
				}
				view.remove_prefix(code_start + CODE_START.size());
				std::string code = "namespace Algiz { template <typename T> void echo(const T &); void _exec_() {";
				const size_t initial_size = code.size();
				code += view;
				code += "} }";
				auto action = std::make_unique<PreprocessAction>(code, delimiter_end, Delimiter::Code);
				bool success = clang::tooling::runToolOnCode(std::move(action), llvm::Twine(code));
				if (success) {
					throw std::runtime_error("Missing end code delimiter");
				}
				if (delimiter_end) {
					size_t valid_size = *delimiter_end - initial_size;
					build_stream << view.substr(0, valid_size) << '\n';
					view.remove_prefix(valid_size + CODE_END.size());
				} else {
					throw std::runtime_error("Syntax error");
				}
			} else if (raw_start < code_start) {
				if (raw_start != 0) {
					build_stream << "echo(\"" << escape(view.substr(0, raw_start)) << "\");\n";
				}
				view.remove_prefix(raw_start + RAW_START.size());
				std::string_view shortened = view.substr(0, findLast(view, RAW_END));
				shortened = view;
				INFO("[[[[[[");
				std::cerr << shortened << "\n";
				INFO("]]]]]]");
				auto action = std::make_unique<PreprocessAction>(shortened, delimiter_end, Delimiter::Raw);
				bool success = clang::tooling::runToolOnCode(std::move(action), llvm::Twine(shortened));
				if (success) {
					throw std::runtime_error("Missing end raw delimiter");
				}
				if (delimiter_end) {
					size_t valid_size = *delimiter_end;
					build_stream << view.substr(0, valid_size);
					view.remove_prefix(valid_size + RAW_END.size());
				} else {
					throw std::runtime_error("Syntax error");
				}
			} else {
				if (!view.empty()) {
					build_stream << "echo(\"" << escape(view) << "\");\n";
				}
				break;
			}
		}


		bool success = true;

		if (!success) {
			throw std::runtime_error("Failed to preprocess module");
		}

		return std::move(build_stream).str();
	}

	void DiagnosticConsumer::HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic &diagnostic) {
		llvm::SmallVector<char> message;
		diagnostic.FormatDiagnostic(message);
		const clang::DiagnosticsEngine *diagnostics = diagnostic.getDiags();
		assert(diagnostics != nullptr);
		auto ids = diagnostics->getDiagnosticIDs();
		unsigned category = ids->getCategoryNumberForDiag(diagnostic.getID());

		ERROR("level[" << int(level) << "]");

		if (level == clang::DiagnosticsEngine::Level::Error || level == clang::DiagnosticsEngine::Level::Fatal) {
			ERROR(std::string_view(message.data(), message.size()) << " [" << ids->getCategoryNameFromID(category).str() << "]");
			if (category == clang::diag::DiagCat_Parse_Issue || category == clang::diag::DiagCat_Lexical_or_Preprocessor_Issue) {
				INFO("maybe");
				clang::SourceManager &manager = action.getCompilerInstance().getSourceManager();
				const clang::SourceLocation &location = diagnostic.getLocation();
				const unsigned offset = manager.getFileOffset(location);
				// We need enough space for end token.
				std::string_view end = action.delimiter == Delimiter::Code? CODE_END : RAW_END;
				if (offset + end.size() <= action.code.size()) {
					if (action.code.substr(offset, end.size()) == end) {
						if (!action.delimiterEnd) {
							SUCCESS("delimiterEnd set to " << offset << ".");
							ERROR(std::string_view(message.data(), message.size()));
							action.delimiterEnd = offset;
							return;
						}
					} else {
						WARN("it doesn't match. {" << action.code.substr(offset, end.size()) << "} vs {" << end << "}");
					}
				} else {
					WARN("not enough space");
				}
			}

		}


		diagnostic.getLocation().dump(action.getCompilerInstance().getSourceManager());
		clang::DiagnosticConsumer::HandleDiagnostic(level, diagnostic);
	}
}

namespace {
	const auto _ = [] {
		std::string result = Algiz::Plugins::preprocessModule("./www/foo.alg");
		INFO("result:\n" << result);
		std::exit(0);
		return 0;
	}();
}