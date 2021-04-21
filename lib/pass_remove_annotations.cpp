#include "pass_remove_annotations.hh"

#include <minizinc/astiterator.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <string>
#include <vector>

using namespace MiniZinc;
using std::string;
using std::vector;

RemoveAnnotations::RemoveAnnotations(const std::vector<string> &as)
    : ann_names{as} {}

MiniZinc::Env *RemoveAnnotations::run(MiniZinc::Env *e, std::ostream &log) {
  Model *m = e->model();

  class AnnotationRemover : public EVisitor {
  private:
    vector<string> &ann_names;

  public:
    AnnotationRemover(vector<string> &as) : ann_names{as} {}
    bool enter(Expression *e) {
      if (!e)
        return false;
      vector<Expression *> toRemove;
      for (Expression *ann_e : e->ann()) {
        if (ann_names.empty()) {
          toRemove.push_back(ann_e);
        } else {
          for (string &name : ann_names) {
            if ((ann_e->isa<Id>() &&
                 string(ann_e->cast<Id>()->str().c_str()) == name) ||
                (ann_e->isa<Call>() &&
                 string(ann_e->cast<Call>()->id().c_str()) == name)) {
              toRemove.push_back(ann_e);
              break;
            }
          }
        }
      }
      for (Expression *ann_e : toRemove) {
        e->ann().remove(ann_e);
      }
      return true;
    }
  } remover(ann_names);

  for (VarDeclI &vdi : m->vardecls()) {
    top_down(remover, vdi.e());
  }

  for (ConstraintI &ci : m->constraints()) {
    top_down(remover, ci.e());
  }

  return e;
}
