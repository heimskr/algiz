#include "util/Util.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "plugins/fileserv/Fileserv.h"

#include <array>

namespace Algiz::Plugins {
	class PreprocessConsumer final: public clang::ASTConsumer {
		public:
			void Initialize(clang::ASTContext &) override {
				INFO("initializing!");
			}

			void HandleTranslationUnit(clang::ASTContext &) override {
				INFO("handling!");
			}
	};

	class PreprocessAction final: public clang::ASTFrontendAction {
		public:
			PreprocessAction(std::string &output):
				output(output) {}

		protected:
			std::string &output;

			std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &, llvm::StringRef in_file) override {
				INFO("CreateASTConsumer! " << in_file.str() << " in " << std::filesystem::current_path());
				return std::make_unique<PreprocessConsumer>();
			}

			void ExecuteAction() override {
				INFO("ExecuteAction!");
				output.clear();

				clang::CompilerInstance &instance = getCompilerInstance();
				auto maybe_buffer = instance.getFileManager().getBufferForFile(getCurrentFile());
				if (!maybe_buffer || !*maybe_buffer) {
					throw std::runtime_error("Couldn't open module to preprocess");
				}

				llvm::MemoryBuffer &memory_buffer = **maybe_buffer;
				std::string buffer(memory_buffer.getBuffer());
				std::string_view view(buffer);

				constexpr std::string_view code_start_string = "<?";

				for (;;) {
					size_t code_start = view.find(code_start_string);
					output += "echo(\"" + escape(view.substr(0, code_start)) + "\");\n";
					if (code_start == std::string::npos) {
						break;
					}
					view.remove_prefix(code_start + code_start_string.size());
				}

				// clang::ASTFrontendAction::ExecuteAction();
			}

		private:
			bool BeginSourceFileAction(clang::CompilerInstance &) override {
				INFO("BeginSourceFileAction!");
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
		std::string output = R"(
extern "C" void algizModule(HTTP::Server::HandlerArgs &args) {
	auto &[http, client, request, parts] = args;
	http.server->send(client.id, HTTP::Response(200, "preprocessing not yet implemented"));
}
		)";
		auto action = std::make_unique<PreprocessAction>(output);
		const std::filesystem::path old_cwd = std::filesystem::current_path();
		std::string source = readFile(path);
		std::filesystem::copy_file(path, tempdir / "input.alg");
		std::filesystem::current_path(tempdir);
		bool success = clang::tooling::runToolOnCode(std::move(action), std::move(source));
		std::filesystem::current_path(old_cwd);

		if (!success) {
			throw std::runtime_error("Failed to preprocess module");
		}

		return output;
	}
}

namespace {
	const auto _ = [] {
		// return std::atexit([] {
			std::string result = Algiz::Plugins::preprocessModule("./www/foo.alg");
			INFO("result:\n" << result);
		// });
		std::exit(0);
		return 0;
	}();
}