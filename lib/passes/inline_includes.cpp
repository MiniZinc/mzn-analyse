#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <minizinc/astiterator.hh>
#include <minizinc/copy.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>

#include "passes/inline_includes.hh"

using namespace MiniZinc;
using std::string;
using std::stringstream;
using std::unordered_map;
using std::vector;

namespace MznTool {

InlineIncludes::InlineIncludes(bool lo) : local_only{lo} {}

Env *InlineIncludes::run(Env *e, std::ostream &log) {
  Model *model = e->model();
  string mzn_stdlib_dir = FileUtils::file_path(FileUtils::share_directory());

  // Add data annotations
  for (size_t i = 0; i < model->size(); i++) {
    Item *item = model->operator[](i);
    if (IncludeI *ii = item->dynamicCast<IncludeI>()) {
      string filepath = ii->m()->filepath().c_str();
      if (!local_only || filepath.rfind(mzn_stdlib_dir, 0) != 0) {
        ii->remove();
        Model *im = ii->m();
        for (size_t j = 0; j < im->size(); j++) {
          model->addItem(im->operator[](j));
        }
      }
    }
  }
  model->compact();
  return e;
}
}; // namespace MznTool
