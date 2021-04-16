#include "pass_remove_annotations.hh"

#include <minizinc/astiterator.hh>
#include <minizinc/model.hh>
#include <string>
#include <vector>

using namespace MiniZinc;
using std::string;
using std::vector;

RemoveAnnotations::RemoveAnnotations(std::string a) { ann_names.push_back(a); }

RemoveAnnotations::RemoveAnnotations(std::vector<string> &as) : ann_names{as} {}

MiniZinc::Env *RemoveAnnotations::run(MiniZinc::Env *e, std::ostream &log) {
  Model *m = e->model();

  class AnnotationRemover : public EVisitor {
  private:
    vector<string> &ann_names;

  public:
    AnnotationRemover(vector<string> &as) : ann_names{as} {}
    bool enter(Expression *e) {
      for (string &name : ann_names) {
        e->ann().removeCall(ASTString(name));
      }
      return true;
    }
  } remover(ann_names);

  for (ConstraintI &ci : m->constraints()) {
    top_down(remover, ci.e());
  }

  return e;
}
