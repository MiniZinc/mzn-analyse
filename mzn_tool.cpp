#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "pass_annotate_data_deps.hh"
#include "pass_get_data_deps.hh"
#include "pass_get_term_types.hh"
#include "pass_inline_includes.hh"
#include "pass_remove_annotations.hh"
#include "pass_remove_includes.hh"
#include "pass_remove_items.hh"
#include "tool_pass.hh"

#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/solver.hh>

using namespace MiniZinc;

using std::string;
using std::unique_ptr;
using std::vector;

void parse_path(Env &env, string &mzn_path, bool is_fzn) {
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
  vector<unique_ptr<MiniZinc::Pass>> passes;

  if (argc == 1) {
    std::cerr << "Incorrect number of arguments" << std::endl;
    return EXIT_FAILURE;
  }

  //        0         1      2         3           4
  // mzn_tool  annotate in.mzn [out.mzn]
  // mzn_tool get_terms in.mzn [out.mzn] [out.terms]
  // mzn_tool  get_data in.fzn [out.fzn]  [out.cons]

  if (argc < 3) {
    std::cerr << "Incorrect number of arguments\n";
    return EXIT_FAILURE;
  }
  string cmd = argv[1];
  string in_path = argv[2];
  string extension = in_path.substr(in_path.size() - 4, string::npos);
  bool is_fzn = extension == ".fzn";

  string output_base = in_path.substr(0, in_path.size() - 4);

  string out_path;
  if (argc >= 4) {
    out_path = argv[3];
  }

  string extra_arg;
  if (argc >= 5) {
    extra_arg = argv[4];
  }

  if (cmd == "annotate") {
    if (out_path.empty()) {
      out_path = output_base + ".annotated.mzn";
    }
    passes.emplace_back(new InlineIncludes());
    passes.emplace_back(new AnnotateDataDeps());
  } else if (cmd == "get_terms") {
    if (out_path.empty()) {
      out_path = output_base + ".solveless.mzn";
    }
    if (extra_arg.empty()) {
      extra_arg = output_base + ".terms";
    }

    passes.emplace_back(new GetTermTypes(extra_arg));
    passes.emplace_back(new RemoveAnnotations({"data"}));
    passes.emplace_back(new RemoveItems({Item::II_SOL, Item::II_OUT}));
  } else if (cmd == "get_data") {
    if (!is_fzn) {
      std::cerr << "get_data must take a fzn file as input" << std::endl;
      return EXIT_FAILURE;
    }
    if (out_path.empty()) {
      out_path = output_base + ".noanns.fzn";
    }
    if (extra_arg.empty()) {
      extra_arg = output_base + ".cons";
    }
    passes.emplace_back(new GetDataDeps(extra_arg));
    passes.emplace_back(new RemoveAnnotations({"data"}));
  } else {
    std::cerr << "Unknown command: " << cmd << std::endl;
    std::cerr << "  valid commands are: annotate, get_terms, get_data"
              << std::endl;
    return EXIT_FAILURE;
  }
  passes.emplace_back(
      new RemoveIncludes({"solver_redefinitions.mzn", "stdlib.mzn"}));

  GCLock lock;
  Env env;
  parse_path(env, in_path, is_fzn);

  Env *out_env = multiPassFlatten(env, passes, std::cerr);

  std::cerr << "Writing output to: " << out_path << std::endl;
  std::ofstream of(out_path);
  Printer pp(of, is_fzn ? 0 : 80, is_fzn);
  pp.print(out_env->model());

  return EXIT_SUCCESS;
}
