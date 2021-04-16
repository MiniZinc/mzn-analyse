#include <fstream>
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

using namespace MiniZinc;
using std::string;
using std::stringstream;
using std::unordered_map;
using std::vector;

bool isStandardInclude(IncludeI& ii) {
  string filename = ii.f().c_str();
  return filename == "solver_redefinitions.mzn"
    || filename == "stdlib.mzn";
}
bool isLocalInclude(IncludeI& ii) {
  string mzn_stdlib_dir = FileUtils::share_directory();
  string filepath = ii.m()->filepath().c_str();
  return !filepath.rfind(mzn_stdlib_dir, 0) == 0;
}

namespace MznData {
void inline_local_includes(MiniZinc::Model *model, std::string& output) {
  // Collect functional assignments for objective processing
  unordered_map<Id *, Expression *> assigns;

  // Add data annotations
  size_t orig_size = model->size();
  for (size_t i = 0; i < model->size(); i++) {
    Item* item = model->operator[](i);
    if(IncludeI *ii = item->dynamicCast<IncludeI>()) {
      if(isStandardInclude(*ii)) {
        ii->remove();
      } else if(isLocalInclude(*ii)) {
        ii->remove();
        Model* im = ii->m();
        for(size_t j = 0; j < im->size(); j++) {
          model->addItem(im->operator[](j));
        }
      }
    }
  }
  model->compact();

  {
    std::ofstream of(output);
    std::cerr << "Writing model with inlined local includes to: " << output
              << std::endl;
    Printer pp(of, 80, false);
    pp.print(model);
    of.close();
  }
}
}; // namespace MznData
