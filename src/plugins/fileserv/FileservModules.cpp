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

	std::string preprocessModule(const std::filesystem::path &path) {
		char tempdir_template[]{"/tmp/algiz-pp-XXXXXX"};
		if (mkdtemp(tempdir_template) == nullptr) {
			throw std::runtime_error("Couldn't create temporary directory for module preprocessing");
		}
		std::filesystem::path tempdir{tempdir_template};
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
		std::stringstream stream;

		for (;;) {
			std::optional<size_t> delimiter_end;
			size_t code_start = view.find(CODE_START);

			if (code_start == std::string::npos) {
				if (!view.empty()) {
					stream << "echo(\"" << escape(view) << "\");\n";
				}

				break;
			}

			if (code_start != 0) {
				stream << "echo(\"" << escape(view.substr(0, code_start)) << "\");\n";
			}

			view.remove_prefix(code_start + CODE_START.size());

			size_t last = findLast(view, CODE_END);
			if (last < std::string::npos) {
				last += CODE_END.size();
			}
			std::string_view shortened = view.substr(0, last);

			auto action = std::make_unique<PreprocessAction>(shortened, delimiter_end);
			clang::tooling::runToolOnCode(std::move(action), llvm::Twine(shortened));

			if (delimiter_end) {
				size_t valid_size = *delimiter_end;
				stream << shortened.substr(0, valid_size) << '\n';
				view.remove_prefix(valid_size + CODE_END.size());
			} else {
				throw std::runtime_error("Syntax error");
			}
		}

		return std::move(stream).str();
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

namespace {
	const auto _ = [] {
		std::string result = Algiz::Plugins::preprocessModule("./www/foo.alg");
		INFO("result:\n" << result);
		std::exit(0);
		return 0;
	}();
}