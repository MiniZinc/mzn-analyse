#include "passes/get_exprs.hh"

#include <algorithm>
#include <iostream>
#include <minizinc/astiterator.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>
#include <string>
#include <vector>

#include "location_utils.hh"
#include "string_utils.hh"

using namespace MiniZinc;
using std::ostream;
using std::string;
using std::vector;

namespace MznAnalyse {

UniqueCollector::UniqueCollector(const std::vector<ShortLoc>& locations, bool typed) : locs{locations}, include_types {typed} {}

UniqueCollector::UniqueCollector(const std::vector<std::string>& paths, bool typed) : include_types {typed} {
  for (const string& path : paths) {
    locs.emplace_back(path);
  }
}

void UniqueCollector::add_expr(const ShortLoc& loc, Expression* e) {
  std::stringstream ss_repr;
  MiniZinc::Printer p(ss_repr, 0, true);
  p.print(e);

  std::string repr = ss_repr.str();
  if (repr.empty()) return;
  if (repr[0] == '"') return;

  std::stringstream ss_type;
  TypeInst* ti = nullptr;
  if (Id* id = Expression::dynamicCast<Id>(e)) {
    VarDecl* typed = id->decl();
    ti = typed->ti();
  } else {
    ti = new TypeInst(Location().introduce(), Expression::type(e));
  }

  ss_type << *ti;
  std::string type = ss_type.str();

  std::string loc_key = loc.to_string();
  std::unordered_map<std::string, ExprInfo>& seen = exprs[loc_key];
  if (seen.find(repr) == seen.end()) {
    seen.insert(std::make_pair(repr, ExprInfo(repr, type)));
  }
}

void UniqueCollector::write_json(ostream& os) {
  std::vector<std::string> entries;

  for (auto& loc_exprs : exprs) {
    std::vector<std::string> unique_exprs;
    for (const auto& expr_info : loc_exprs.second) {
      if(include_types) {
        std::stringstream ss;
        ss << "{ \"expression\": \"" << utils::escape(expr_info.second.repr, false) << "\", ";
        ss << " \"type\": \"" << utils::escape(expr_info.second.type, false) << "\" }";
        unique_exprs.push_back(ss.str());
      } else {
        unique_exprs.push_back(utils::escape(expr_info.second.repr, false));
      }
    }

    std::stringstream entry_ss;
    entry_ss << "    \"" << loc_exprs.first << "\": [";
    entry_ss << utils::join(unique_exprs, ",", !include_types);
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

bool isAnn(Expression* e) {
  if (Call* c = Expression::dynamicCast<Call>(e)) {
    return c->id() == "mzn_constraint_name" || c->id() == "mzn_expression_name";
  }
  return false;
}

ExpressionExtractorEVisitor::ExpressionExtractorEVisitor(const std::vector<ShortLoc>& locations,
                                                         LocExprMap& expr_store, bool only_par,
                                                         bool only_top)
    : locs{locations}, exprs{expr_store}, collect_par{only_par}, no_sub_exprs{only_top} {}

bool ExpressionExtractorEVisitor::enter(Expression* e) {
  if (e == nullptr || isLit(Expression::eid(e)) || isAnn(e)) return false;

  ShortLoc this_loc{Expression::loc(e)};
  bool is_parent = locs.empty() && !no_sub_exprs;
  bool is_child = locs.empty() && !no_sub_exprs;

  std::vector<ShortLoc> parents;

  for (const ShortLoc& loc : locs) {
    if (this_loc == loc) {
      is_parent = true;
      is_child = true;
      parents.push_back(loc);
    } else {
      if (this_loc.contains(loc)) {
        is_parent = true;
      }
      if (loc.contains(this_loc)) {
        is_child = true;
        parents.push_back(loc);
      }
    }
  }

  if (is_child && !(Expression::isa<VarDecl>(e) || Expression::isa<TypeInst>(e))) {
    if (!collect_par || Expression::type(e).ti() == MiniZinc::Type::TI_PAR) {
      if (parents.empty()) {
        exprs[this_loc.to_string()].push_back(e);
        if (no_sub_exprs && !locs.empty()) {
          locs.erase(std::find(locs.begin(), locs.end(), this_loc));
        }
      } else {
        for (const ShortLoc& loc : parents) {
          exprs[loc.to_string()].push_back(e);
          if (no_sub_exprs && !locs.empty()) {
            locs.erase(std::find(locs.begin(), locs.end(), loc));
          }
        }
      }
    }
  }

  return (is_parent || is_child); // && Expression::eid(e) != Expression::E_ARRAYACCESS;
}

ExpressionExtractor::ExpressionExtractor(const std::vector<ShortLoc>& locations,
                                         LocExprMap& expr_store, bool only_par, bool only_top)
    : eev{locations, expr_store, only_par, only_top} {}

bool ExpressionExtractor::enter(Item* item) { return !item->isa<IncludeI>(); }
void ExpressionExtractor::vVarDeclI(VarDeclI* vdi) {
  VarDecl* vd = vdi->e();
  top_down(eev, vd);
}
void ExpressionExtractor::vAssignI(AssignI* ai) {
  top_down(eev, ai->decl());
  top_down(eev, ai->e());
}
void ExpressionExtractor::vConstraintI(ConstraintI* ci) { top_down(eev, ci->e()); }
void ExpressionExtractor::vSolveI(SolveI* si) { top_down(eev, si->e()); }
void ExpressionExtractor::vFunctionI(FunctionI* fi) {}

LocExprMap get_exprs(const std::vector<ShortLoc>& locs, Model* m, bool only_par, bool only_top) {
  LocExprMap exprs;

  if (only_top) {
    for (const ShortLoc& loc : locs) {
      std::vector<ShortLoc> tmp_locs;
      tmp_locs.push_back(loc);
      ExpressionExtractor ee{tmp_locs, exprs, only_par, true};
      iter_items(ee, m);
    }
  } else {
    ExpressionExtractor ee{locs, exprs, only_par, false};
    iter_items(ee, m);
  }

  return exprs;
}

GetExprs::GetExprs(const std::vector<std::string>& paths, bool only_par, bool typed)
    : uc{paths, typed}, collect_par{only_par}, include_types{typed} {}

void GetExprs::write_json(ostream& os) { uc.write_json(os); }

std::string GetExprs::get_name() { return "get-exprs"; }

MiniZinc::Env* GetExprs::run(MiniZinc::Env* e, std::ostream& log) {
  LocExprMap expr_map = get_exprs(uc.locs, e->model(), collect_par);
  for (auto& exprs : expr_map) {
    for (Expression* e : exprs.second) {
      uc.add_expr(exprs.first, e);
    }
  }

  return e;
}
}  // namespace MznAnalyse
