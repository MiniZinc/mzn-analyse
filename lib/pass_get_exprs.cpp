#include "pass_get_exprs.hh"
#include "location_utils.hh"
#include "string_utils.hh"

#include <algorithm>
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

UniqueCollector::UniqueCollector(const std::vector<ShortLoc> &locations)
    : locs{locations} {}

UniqueCollector::UniqueCollector(const std::vector<std::string> &paths) {
  for (const string &path : paths) {
    locs.emplace_back(path);
  }
}

void UniqueCollector::add_expr(const ShortLoc &loc, std::string s) {
  if (s.empty())
    return;
  if (s[0] == '"')
    return;

  std::string loc_key = loc.to_string();
  std::unordered_set<std::string> &seen = exprs[loc_key];
  if (seen.find(s) == seen.end()) {
    seen.insert(s);
  }
}

void UniqueCollector::add_expr(const ShortLoc &loc, Expression *e) {
  std::stringstream ss;
  MiniZinc::Printer p(ss, 0, true);
  p.print(e);
  add_expr(loc, ss.str());
}

void UniqueCollector::write_json(ostream &os) {
  std::vector<std::string> entries;

  for (auto &loc_exprs : exprs) {
    std::vector<std::string> unique_exprs;
    for (const auto &expr_str : loc_exprs.second) {
      unique_exprs.push_back(utils::escape(expr_str, false));
    }

    std::stringstream entry_ss;
    entry_ss << "    \"" << loc_exprs.first << "\": [";
    entry_ss << utils::join(unique_exprs, ",", true);
    entry_ss << "]";
    entries.push_back(entry_ss.str());
  }

  os << "{\n";
  os << utils::join(entries, ",\n", false);
  os << "}";
}

bool isLit(Expression::ExpressionId eid) {
  return eid == Expression::E_FLOATLIT || eid == Expression::E_INTLIT ||
         eid == Expression::E_BOOLLIT || eid == Expression::E_STRINGLIT ||
         eid == Expression::E_SETLIT;
}

bool isAnn(Expression *e) {
  if (Call *c = e->dynamicCast<Call>()) {
    return c->id() == "mzn_constraint_name" || c->id() == "mzn_expression_name";
  }
  return false;
}

struct ExpressionExtractorEVisitor : public EVisitor {
  UniqueCollector &p;
  const std::vector<ShortLoc> &locs;

  ExpressionExtractorEVisitor(UniqueCollector &p1,
                              const std::vector<ShortLoc> &locations)
      : p{p1}, locs{locations} {}

  bool enter(Expression *e) {
    if (e == nullptr || isLit(e->eid()) || isAnn(e))
      return false;

    ShortLoc this_loc{e->loc()};
    bool is_parent = locs.empty();
    bool is_child = locs.empty();

    std::vector<ShortLoc> parents;

    for (const ShortLoc &loc : locs) {
      if (this_loc.contains(loc)) {
        is_parent = true;
      }
      if (loc.contains(this_loc)) {
        is_child = true;
        parents.push_back(loc);
      }
    }

    if (is_child && !(e->isa<VarDecl>() || e->isa<TypeInst>())) {
      if (e->type().ti() == MiniZinc::Type::TI_PAR) {
        if (parents.empty()) {
          p.add_expr(this_loc, e);
        } else {
          for (const ShortLoc &loc : parents) {
            p.add_expr(loc, e);
          }
        }
      }
    }

    bool do_enter =
        (is_parent || is_child) && e->eid() != Expression::E_ARRAYACCESS;

    return do_enter;
  }
};

struct ExpressionExtractor : public ItemVisitor {
  UniqueCollector &p;
  ExpressionExtractorEVisitor eev;

  ExpressionExtractor(UniqueCollector &uc) : p{uc}, eev{p, uc.locs} {}

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

GetExprs::GetExprs(const std::vector<std::string> &paths) : uc{paths} {}

void GetExprs::write_json(ostream &os) { uc.write_json(os); }

std::string GetExprs::get_name() { return "get-exprs"; }

MiniZinc::Env *GetExprs::run(MiniZinc::Env *e, std::ostream &log) {
  ExpressionExtractor ee{uc};
  Model *m = e->model();
  iter_items(ee, m);

  return e;
}
