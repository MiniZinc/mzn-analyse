#include "pass_read_model.hh"

#include <fstream>
#include <string>

#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>

using namespace MiniZinc;
using std::string;
using std::vector;

ReadModel::ReadModel(const string &ip) : in_path{ip} {}

Env *ReadModel::run(Env *e, std::ostream &log) {
  Env *nenv = new Env;

  string extension = in_path.substr(in_path.size() - 4, string::npos);
  bool is_fzn = extension == ".fzn";

  vector<string> includes;

  string mzn_stdlib_dir = FileUtils::share_directory();
  includes.push_back(mzn_stdlib_dir + "/std/");

  vector<string> model_paths(1);
  model_paths[0] = in_path;

  Model *m = parse(*nenv, model_paths, {}, "", "", includes, is_fzn, false,
                   false, false, std::cerr);

  if (!m) {
    std::cerr << "ReadModel: Failed to parse file" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  if (!is_fzn) {
    vector<TypeError> typeErrors;
    try {
      typecheck(*nenv, m, typeErrors, true, true, true);
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

  nenv->model(m);
  return nenv;
}
