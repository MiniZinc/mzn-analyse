#include "pass_remove_output.hh"

#include <string>

using namespace MiniZinc;
using std::string;

RemoveOutput::RemoveOutput() {}

Env* RemoveOutput::run(Env* e, std::ostream& log) {
  Model* model = e->model();
  Item* item = model->outputItem();
  if(item) item->remove();
  model->compact();
  return e;
}

