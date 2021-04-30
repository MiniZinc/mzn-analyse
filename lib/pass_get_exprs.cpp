#include "pass_get_exprs.hh"
#include "string_utils.hh"

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


ShortLoc::ShortLoc(const std::string& full_path_entry) {
  std::vector<std::string> parts = utils::split(full_path_entry, '|');
  if(parts.size() >= 5) {
    model_path = FileUtils::base_name(parts[0]);
    sl = stoul(parts[1]);
    sc = stoul(parts[2]);
    el = stoul(parts[3]);
    ec = stoul(parts[4]);
  } else {
    std::cerr << "Warning: cannot parse: " << full_path_entry << std::endl;
  }
}

ShortLoc::ShortLoc(const MiniZinc::Location& mzn_loc) {
  const char* mpath = mzn_loc.filename().c_str();
  if(mpath != nullptr) {
    model_path = FileUtils::base_name(std::string(mpath));
  }
  sl = mzn_loc.firstLine();
  sc = mzn_loc.firstColumn();
  el = mzn_loc.lastLine();
  ec = mzn_loc.lastColumn();
}

std::string ShortLoc::to_string() const {
  std::stringstream ss;
  ss << model_path
     << "|" << sl << "|" << sc
     << "|" << el << "|" << ec;
  return ss.str();
}

ostream &operator<<(ostream &os, const ShortLoc &a) {
  os << a.to_string();
  return os;
}

bool ShortLoc::contains(const ShortLoc& b) const {
  bool cont = model_path == b.model_path
      && (sl < b.sl || (sl == b.sl && sc <= b.sc))
      && (el > b.el || (el == b.el && ec >= b.ec));
  return cont;
}

struct UniqueCollector {
  std::vector<ShortLoc> locs;
  std::unordered_map<std::string, std::unordered_set<std::string> > exprs;

  UniqueCollector(const std::vector<ShortLoc>& locations) : locs{locations} {}

  void add_expr(const ShortLoc& loc, std::string s) {
    if (s.empty())
      return;
    if (s[0] == '"')
      return;

    std::string loc_key = loc.to_string();
    std::unordered_set<std::string>& seen = exprs[loc_key];
    if (seen.find(s) == seen.end()) {
      seen.insert(s);
    }
  }

  void add_expr(const ShortLoc& loc, Expression *e) {
    std::stringstream ss;
    MiniZinc::Printer p(ss, 0, true);
    p.print(e);
    add_expr(loc, ss.str());
  }

  void write_json(ostream &os) {
    std::vector<std::string> entries;

    for(auto& loc_exprs : exprs) {
      std::vector<std::string> unique_exprs { loc_exprs.second.begin(),
                                              loc_exprs.second.end() };

      std::stringstream entry_ss;
      entry_ss << "'" << loc_exprs.first << "': [";
      entry_ss << utils::join(unique_exprs, ",", true);
      entry_ss << "]";
      entries.push_back(entry_ss.str());
    }

    os << "{";
    os << utils::join(entries, "\n,", false);
    os << "}" << std::endl;
  }
};

bool isLit(Expression::ExpressionId eid) {
  return eid == Expression::E_FLOATLIT ||
    eid == Expression::E_INTLIT ||
    eid == Expression::E_BOOLLIT ||
    eid == Expression::E_STRINGLIT ||
    eid == Expression::E_SETLIT;
}

struct ExpressionExtractorEVisitor : public EVisitor {
  UniqueCollector &p;
  const std::vector<ShortLoc> &locs;

  ExpressionExtractorEVisitor(UniqueCollector &p1,
                              const std::vector<ShortLoc> &locations) : p{p1}, locs{locations} {}

  bool enter(Expression *e) {
    if (e == nullptr || isLit(e->eid())) return false;

    ShortLoc this_loc { e->loc() };
    bool is_parent = locs.empty();
    bool is_child = locs.empty();

    std::vector<ShortLoc> parents;

    for(const ShortLoc& loc : locs) {
      if(this_loc.contains(loc)) {
        is_parent = true;
      }
      if(loc.contains(this_loc)) {
        is_child = true;
        parents.push_back(loc);
      }
    }

    if (is_child && !(e->isa<VarDecl>() || e->isa<TypeInst>())) {
      if (e->type().ti() == MiniZinc::Type::TI_PAR) {
        if(parents.empty()) {
          p.add_expr(this_loc, e);
        } else {
          for(const ShortLoc &loc : parents) {
            p.add_expr(loc, e);
          }
        }
      }
    }

    bool do_enter = (is_parent || is_child) &&
                    e->eid() != Expression::E_ARRAYACCESS;

    return do_enter;
  }
};

struct ExpressionExtractor : public ItemVisitor {
  UniqueCollector &p;
  ExpressionExtractorEVisitor eev;

  ExpressionExtractor(const std::vector<ShortLoc> &locations,
                      UniqueCollector &uc) : p{uc}, eev{p, locations} {}

  bool enter(Item *item) { return !item->isa<IncludeI>(); }
  void vVarDeclI(VarDeclI *vdi) {
    VarDecl *vd = vdi->e();
    top_down(eev, vd);
  }
  void vAssignI(AssignI *ai) {
    top_down(eev, ai->decl());
    top_down(eev, ai->e());
  }
  void vConstraintI(ConstraintI *ci) {
    top_down(eev, ci->e());
  }
  void vSolveI(SolveI *si) { top_down(eev, si->e()); }
  void vFunctionI(FunctionI *fi) {}
};


GetExprs::GetExprs(const std::vector<std::string>& paths) {
  for(const string& path : paths) {
    locs.emplace_back(path);
  }
}

MiniZinc::Env *GetExprs::run(MiniZinc::Env *e, std::ostream &log) {
  Model *m = e->model();
  // Collect and write data entries
  string fzn_path = m->filepath().c_str();

  string output_path = "-";
  if (output_path == "-") {
    UniqueCollector uc {locs};
    ExpressionExtractor ee {locs, uc};
    iter_items(ee, m);
    uc.write_json(std::cout);
  } else {
    // std::cerr << "Writing constraint data to: " << output_path << std::endl;
    // std::ofstream out_json_os{output_path};
    // write_data_deps(m, out_json_os);
    // out_json_os.close();
  }

  return e;
}
