#include <iostream>
#include <string>
#include <vector>

#include <minizinc/astiterator.hh>
#include <minizinc/copy.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>

#include "mzn_data.hh"

using namespace MiniZinc;
using namespace MznData;

using std::string;
using std::vector;

enum Mode { ANNOTATE, EXTRACT, OBJECTIVE };

void parse_path(Env &env, string &mzn_path, bool is_fzn = false) {
  vector<string> includes;
  string mzn_stdlib_dir = FileUtils::share_directory();
  includes.push_back(mzn_stdlib_dir + "/std/");
  vector<string> model_paths(1);
  model_paths[0] = mzn_path;

  Model *m = parse(env, model_paths, {}, "", "", includes, is_fzn, false, false,
                   false, std::cerr);

  if (!m) {
    std::cerr << "mzn_data: Failed to parse file" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  if (!is_fzn) {
    vector<TypeError> typeErrors;
    try {
      typecheck(env, m, typeErrors, true, true, true);
    } catch (TypeError &e) {
      typeErrors.push_back(e);
    }
    if (typeErrors.size() > 0) {
      for (unsigned int i = 0; i < typeErrors.size(); i++) {
        std::cerr << typeErrors[i].loc() << ":" << std::endl;
        std::cerr << typeErrors[i].what() << ":" << typeErrors[i].msg()
                  << std::endl;
      }
      exit(EXIT_FAILURE);
    }
  }

  env.model(m);
}

int main(int argc, char **argv) {
  GCLock lock;

  Mode mode = ANNOTATE;
  string mzn_path;
  for (int i = 1; i < argc; i++) {
    if (string(argv[i]) == "annotate") {
      mode = ANNOTATE;
    } else if (string(argv[i]) == "extract") {
      mode = EXTRACT;
    } else if (string(argv[i]) == "objective") {
      mode = OBJECTIVE;
    } else {
      if (!mzn_path.empty()) {
        std::cerr << "Warning ignoring previous path: " << mzn_path
                  << std::endl;
      }
      mzn_path = argv[i];
    }
  }

  Env env;

  if (mode == ANNOTATE) {
    parse_path(env, mzn_path);
    annotate(env.envi(), env.model());
  } else if (mode == EXTRACT) {
    parse_path(env, mzn_path, true);
    extract(env.model());
  } else if (mode == OBJECTIVE) {
    parse_path(env, mzn_path);
    objective(env.model());
  }

  return EXIT_SUCCESS;
}
