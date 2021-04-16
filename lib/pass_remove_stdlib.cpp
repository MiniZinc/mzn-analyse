#include "pass_remove_stdlib.hh"

#include <string>

using namespace MiniZinc;
using std::string;

RemoveStdlib::RemoveStdlib() {}

bool isStandardInclude(IncludeI &ii) {
  string filename = ii.f().c_str();
  return filename == "solver_redefinitions.mzn" || filename == "stdlib.mzn";
}

Env *RemoveStdlib::run(Env *e, std::ostream &log) {
  Model *model = e->model();

  for (size_t i = 0; i < model->size(); i++) {
    Item *item = model->operator[](i);
    if (IncludeI *ii = item->dynamicCast<IncludeI>()) {
      if (isStandardInclude(*ii)) {
        ii->remove();
      }
    }
  }
  model->compact();
  return e;
}
