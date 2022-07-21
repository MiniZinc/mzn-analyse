#include "passes/remove_includes.hh"

#include <string>

using namespace MiniZinc;
using std::string;

namespace MznTool {

RemoveIncludes::RemoveIncludes(const std::vector<string>& is) : includes{is} {}

Env* RemoveIncludes::run(Env* e, std::ostream& log) {
  Model* model = e->model();

  for (size_t i = 0; i < model->size(); i++) {
    Item* item = model->operator[](i);
    if (IncludeI* ii = item->dynamicCast<IncludeI>()) {
      if (includes.empty()) {
        ii->remove();
      } else {
        for (string& iname : includes) {
          if (ii->f() == iname) {
            ii->remove();
            break;
          }
        }
      }
    }
  }
  model->compact();
  return e;
}
};  // namespace MznTool
