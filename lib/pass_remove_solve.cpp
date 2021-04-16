#include "pass_remove_solve.hh"

#include <string>

using namespace MiniZinc;
using std::string;

RemoveSolve::RemoveSolve() {}

Env *RemoveSolve::run(Env *e, std::ostream &log) {
  Model *model = e->model();
  Item *item = model->solveItem();
  if (item)
    item->remove();
  model->compact();
  return e;
}
