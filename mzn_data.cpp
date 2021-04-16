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

enum Mode { ANNOTATE, EXTRACT, OBJECTIVE, INLINE_LOCAL_INCLUDES };

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
  string output_1;
  string output_2;

  int cmd_idx = 1;
  if (string(argv[cmd_idx]) == "annotate") {
    mode = ANNOTATE;
  } else if (string(argv[cmd_idx]) == "extract") {
    mode = EXTRACT;
  } else if (string(argv[cmd_idx]) == "objective") {
    mode = OBJECTIVE;
  } else if (string(argv[cmd_idx]) == "inline-includes" ||
             string(argv[cmd_idx]) == "ii") {
    mode = INLINE_LOCAL_INCLUDES;
  } else {
    std::cerr << "mzn_data: Unknown command: " << argv[cmd_idx] << std::endl;
    return EXIT_FAILURE;
  }

  for (int i = 2; i < argc; i++) {
    if(mzn_path.empty()) {
      mzn_path = argv[i];
    } else if(output_1.empty()) {
      output_1 = argv[i];
    } else if(output_2.empty()) {
      output_2 = argv[i];
    } else {
      std::cerr << "mzn_data: Too many arguments" << std::endl;
      return EXIT_FAILURE;
    }
  }

  Env env;
  string output_base = mzn_path.substr(0, mzn_path.size() - 4);
  if (mode == ANNOTATE) {
    parse_path(env, mzn_path);
    string annotated_model_path = output_base + "_annotated.mzn";
    if(!output_1.empty()) {
      annotated_model_path = output_1;
    }
    annotate(env.envi(), env.model(), annotated_model_path);
  } else if (mode == INLINE_LOCAL_INCLUDES) {
    parse_path(env, mzn_path);
    string inlined_model_path = output_base + "_inlined.mzn";
    if(!output_1.empty()) {
      inlined_model_path = output_1;
    }
    inline_local_includes(env.model(), inlined_model_path);
  } else if (mode == EXTRACT) {
    parse_path(env, mzn_path, true);
    string out_json = output_base + ".cons";
    if(!output_1.empty()) {
      out_json = output_1;
    }
    extract(env.model(), out_json);
  } else if (mode == OBJECTIVE) {
    parse_path(env, mzn_path);
    string model_output = output_base + ".solveless.mzn";
    if(!output_1.empty()) {
      model_output = output_1;
    }
    string json_output = output_base + ".terms.json";
    if(!output_2.empty()) {
      json_output = output_2;
    }
    objective(env.model(), model_output, json_output);
  }

  return EXIT_SUCCESS;
}
