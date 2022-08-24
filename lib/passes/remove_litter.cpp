#include "passes/remove_litter.hh"

#include <iostream>
#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <string>

using namespace MiniZinc;
using std::string;

namespace MznTool {

RemoveLitter::RemoveLitter() {}

Env* RemoveLitter::run(Env* e, std::ostream& log) {
  Model* model = e->model();
  string mzn_stdlib_dir = FileUtils::file_path(FileUtils::share_directory());

  // Add data annotations
  for (size_t i = 0; i < model->size(); i++) {
    Item* item = model->operator[](i);
    ASTString filepath = item->loc().filename();
    if (filepath.empty() || string(filepath.c_str()).rfind(mzn_stdlib_dir, 0) == 0) {
      item->remove();
    }
  }
  model->compact();
  return e;
}
};  // namespace MznTool
