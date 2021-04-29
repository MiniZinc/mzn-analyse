#include "pass_get_exprs.hh"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <minizinc/astiterator.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>

using namespace MiniZinc;
using std::ostream;
using std::string;
using std::vector;

struct UniquePrinter {
  std::unordered_set<string> seen;

  void print(std::string s) {
    if (s.empty())
      return;
    if (s[0] == '"')
      return;
    if (seen.find(s) == seen.end()) {
      seen.insert(s);
      std::cout << s << "\n";
    }
  }
  void print(Expression *e) {
    std::stringstream ss;
    MiniZinc::Printer p(ss, 0, true);
    p.print(e);
    std::string s = ss.str();
    print(s);
  }
};

struct ExpressionExtractorEVisitor : public EVisitor {
  UniquePrinter &p;

  ExpressionExtractorEVisitor(UniquePrinter &p1) : p{p1} {}

  bool enter(Expression *e) {
    if (e && !(e->isa<VarDecl>() || e->isa<TypeInst>())) {
      if (e->type().ti() == MiniZinc::Type::TI_PAR &&
          e->eid() != Expression::E_FLOATLIT &&
          e->eid() != Expression::E_INTLIT &&
          e->eid() != Expression::E_BOOLLIT &&
          e->eid() != Expression::E_STRINGLIT &&
          e->eid() != Expression::E_SETLIT) {
        p.print(e);
      }
    }

    bool do_enter = e != nullptr &&
                    e->eid() != Expression::E_ARRAYACCESS;

    return do_enter;
  }
};

struct ExpressionExtractor : public ItemVisitor {
  UniquePrinter p;
  ExpressionExtractorEVisitor eev;
  ExpressionExtractor() : eev{p} {}

  bool enter(Item *item) { return !item->isa<IncludeI>(); }
  void vVarDeclI(VarDeclI *vdi) {
    VarDecl *vd = vdi->e();
    top_down(eev, vd);
  }
  void vAssignI(AssignI *ai) {
    top_down(eev, ai->decl());
    top_down(eev, ai->e());
  }
  void vConstraintI(ConstraintI *ci) { top_down(eev, ci->e()); }
  void vSolveI(SolveI *si) { top_down(eev, si->e()); }
  void vFunctionI(FunctionI *fi) {}
};

GetExprs::GetExprs(const std::string &out_path) : output_path{out_path} {}

MiniZinc::Env *GetExprs::run(MiniZinc::Env *e, std::ostream &log) {
  Model *m = e->model();
  // Collect and write data entries
  string fzn_path = m->filepath().c_str();

  if (output_path == "-") {
    ExpressionExtractor ee;
    iter_items(ee, m);
  } else {
    // std::cerr << "Writing constraint data to: " << output_path << std::endl;
    // std::ofstream out_json_os{output_path};
    // write_data_deps(m, out_json_os);
    // out_json_os.close();
  }

  return e;
}
