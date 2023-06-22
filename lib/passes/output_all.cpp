#include "passes/output_all.hh"

#include <minizinc/astiterator.hh>
#include <minizinc/model.hh>

using namespace MiniZinc;
using std::string;
using std::vector;

namespace MznAnalyse {

OutputAll::OutputAll() {}

MiniZinc::Env* OutputAll::run(MiniZinc::Env* e, std::ostream& log) {
  Model* m = e->model();

  for (VarDeclI& vdi : m->vardecls()) {
    VarDecl* vd = vdi.e();
    Expression::ann(vd).add(MiniZinc::Constants::constants().ann.add_to_output);
  }

  return e;
}
};  // namespace MznAnalyse
