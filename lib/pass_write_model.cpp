#include "pass_write_model.hh"

#include <string>
#include <fstream>

#include <minizinc/prettyprinter.hh>

using namespace MiniZinc;
using std::string;

WriteModel::WriteModel(const string &op, bool fzn) : out_path{op}, is_fzn{fzn} {}

Env *WriteModel::run(Env *e, std::ostream &log) {
  Model *model = e->model();

  if(out_path != "-") {
    std::cerr << "Writing output to: " << out_path << std::endl;
    std::ofstream of(out_path);
    Printer pp(of, is_fzn ? 0 : 80, is_fzn);
    pp.print(model);
  } else {
    Printer pp(std::cout, is_fzn ? 0 : 80, is_fzn);
    pp.print(model);
  }

  return e;
}
