#include "passes/read_model.hh"

#include <fstream>
#include <iterator>
#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>
#include <string>

using namespace MiniZinc;
using std::string;
using std::vector;

namespace MznTool {

ReadModel::ReadModel(const string& ip) : in_path{ip} {}

Env* ReadModel::run(Env* e, std::ostream& log) {
  Env* nenv = new Env;

  string extension = in_path.size() > 4 ? in_path.substr(in_path.size() - 4, string::npos) : ".mzn";
  bool is_fzn = extension == ".fzn";

  vector<string> includes;

  string mzn_stdlib_dir = FileUtils::share_directory() + "/std/";
  includes.push_back(mzn_stdlib_dir);

  vector<string> model_paths(1);
  model_paths[0] = in_path;

  Model* m = nullptr;

  if (in_path == "-") {
    std::vector<MiniZinc::SyntaxError> syntaxErrors;
    std::string input =
        std::string(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
    m = parse_from_string(*nenv, input, "stdin.mzn", includes, is_fzn, false, false, false,
                          std::cerr);
    if (syntaxErrors.size() > 0) {
      for (unsigned int i = 0; i < syntaxErrors.size(); i++) {
        std::cerr << syntaxErrors[i].loc() << ":" << std::endl;
        std::cerr << syntaxErrors[i].what() << ":" << syntaxErrors[i].msg() << std::endl;
      }
      exit(EXIT_FAILURE);
    }
  } else {
    m = parse(*nenv, model_paths, {}, "", "", includes, {}, is_fzn, false, false, false, std::cerr);
  }

  if (!m) {
    std::cerr << "ReadModel: Failed to parse file: " << in_path << std::endl;
    std::exit(EXIT_FAILURE);
  }
  if (!is_fzn) {
    vector<TypeError> typeErrors;
    try {
      typecheck(*nenv, m, typeErrors, true, true, true);
    } catch (TypeError& e) {
      typeErrors.push_back(e);
    }
    if (typeErrors.size() > 0) {
      for (unsigned int i = 0; i < typeErrors.size(); i++) {
        std::cerr << typeErrors[i].loc() << ":" << std::endl;
        std::cerr << typeErrors[i].what() << ":" << typeErrors[i].msg() << std::endl;
      }
      exit(EXIT_FAILURE);
    }
  }

  nenv->model(m);
  return nenv;
}
};  // namespace MznTool
