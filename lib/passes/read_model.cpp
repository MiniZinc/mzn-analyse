#include "passes/read_model.hh"

#include <fstream>
#include <iterator>
#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>
#include <string>
#include "string_utils.hh"

#include <exception>

using namespace MiniZinc;
using std::string;
using std::vector;

namespace MznTool {

struct LibNotFoundError : public std::exception {
  const char *what() const throw () {
    return "mzn_tool Cannot find minizinc StdLibDir; Try setting MZN_STDLIB_DIR environment variable.\n"
           "           The correct path might be found by calling minizinc --config-dirs \n";
  }
};

ReadModel::ReadModel(const string& ip) : is_fzn{false} {
  string extension = ip.size() > 4 ? ip.substr(ip.size() - 4, string::npos) : ".mzn";
  is_fzn = extension == ".fzn";

  // Assume that the file is mzn/fzn
  mzn_paths.push_back(ip);
}

ReadModel::ReadModel(const vector<string> &ips) : is_fzn{false} {
  for(const string &ip : ips) {
    string extension = ip.size() > 4 ? ip.substr(ip.size() - 4, string::npos) : ".mzn";
    if (extension != ".dzn") {
      is_fzn = extension == ".fzn";
      mzn_paths.push_back(ip);
    } else {
      dzn_paths.push_back(ip);
    }
  }
}

Env* ReadModel::run(Env* e, std::ostream& log) {
  Env* nenv = new Env;

  vector<string> includes;

  string share_dir = FileUtils::share_directory();
  if(share_dir.empty()) {
    throw LibNotFoundError();
  }

  string mzn_stdlib_dir = share_dir + "/std/";
  includes.push_back(mzn_stdlib_dir);

  Model* m = nullptr;

  try {
    if (mzn_paths[0] == "-") {
      std::vector<MiniZinc::SyntaxError> syntaxErrors;
      std::string input =
          std::string(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
      m = parse_from_string(*nenv, input, "stdin.mzn", includes, is_fzn, false, false, false, std::cerr);
      if (syntaxErrors.size() > 0) {
        for (unsigned int i = 0; i < syntaxErrors.size(); i++) {
          std::cerr << syntaxErrors[i].loc() << ":" << std::endl;
          std::cerr << syntaxErrors[i].what() << ":" << syntaxErrors[i].msg() << std::endl;
        }
        exit(EXIT_FAILURE);
      }
    } else {
      m = parse(*nenv, mzn_paths, dzn_paths, "", "", includes, {}, is_fzn, false, false, false, std::cerr);
    }
  } catch (ResultUndefinedError& e) {
    // Ensure warnings are printed, but remove warning corresponding to this error
    std::cerr << "Failed to parse file\n";
    if (nenv != nullptr) {
      nenv->dumpWarnings(std::cerr, false, false, e.warningIdx());
      nenv->clearWarnings();
    }
    std::exit(EXIT_FAILURE);
  } catch (const InternalError& e) {
    std::cerr << "MiniZinc has encountered an internal error. This is a bug." << std::endl;
    std::cerr << "Please file a bug report using the MiniZinc bug tracker." << std::endl;
    std::cerr << "The internal error message was: " << std::endl;
    std::cerr << "\"" << e.msg() << "\"" << std::endl;
  } catch (const Exception& e) {
    e.print(std::cerr);
    std::exit(EXIT_FAILURE);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    std::exit(EXIT_FAILURE);
  } catch (...) {
    // Ensure warnings are printed
    std::cerr << "Failed to parse file\n";
    if (nenv != nullptr) {
      nenv->dumpWarnings(std::cerr, false, false, false);
      nenv->clearWarnings();
    }
    std::exit(EXIT_FAILURE);
  }

  if (!m) {
    std::cerr << "ReadModel: Failed to parse files: " << utils::join(mzn_paths, ",") << "," << utils::join(dzn_paths, ",") << std::endl;
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
