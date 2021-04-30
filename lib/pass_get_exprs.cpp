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

struct UniquePrinter {
  std::unordered_set<string> seen;
  ostream &os;

  UniquePrinter(ostream &output_stream) : os{output_stream} {}

  void print(std::string s) {
    if (s.empty())
      return;
    if (s[0] == '"')
      return;
    if (seen.find(s) == seen.end()) {
      seen.insert(s);
      os << s << "\n";
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

bool isLit(Expression::ExpressionId eid) {
  return eid == Expression::E_FLOATLIT ||
    eid == Expression::E_INTLIT ||
    eid == Expression::E_BOOLLIT ||
    eid == Expression::E_STRINGLIT ||
    eid == Expression::E_SETLIT;
}

struct ExpressionExtractorEVisitor : public EVisitor {
  UniquePrinter &p;
  const std::vector<ShortLoc> &locs;

  ExpressionExtractorEVisitor(UniquePrinter &p1,
                              const std::vector<ShortLoc> &locations) : p{p1}, locs{locations} {}

  bool enter(Expression *e) {
    if (e == nullptr || isLit(e->eid())) return false;

    ShortLoc this_loc { e->loc() };
    bool is_parent = true;
    bool is_child = true;

    if(!locs.empty()) {
      is_parent = std::any_of(locs.begin(), locs.end(), 
          [&](const ShortLoc& loc) {return this_loc.contains(loc);});
      is_child = std::any_of(locs.begin(), locs.end(), 
          [&](const ShortLoc& loc) {return loc.contains(this_loc);});
    }

    if (is_child && !(e->isa<VarDecl>() || e->isa<TypeInst>())) {
      if (e->type().ti() == MiniZinc::Type::TI_PAR) {
        p.print(e);
      }
    }

    bool do_enter = (is_parent || is_child) &&
                    e->eid() != Expression::E_ARRAYACCESS;

    return do_enter;
  }
};

struct ExpressionExtractor : public ItemVisitor {
  UniquePrinter p;
  ExpressionExtractorEVisitor eev;

  ExpressionExtractor(const std::vector<ShortLoc> &locations,
                      ostream &output_stream) : p{output_stream}, eev{p, locations} {}

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

ostream &operator<<(ostream &os, const ShortLoc &a) {
  os << "ShortLoc(" << a.model_path
     << "," << a.sl
     << "," << a.sc
     << "," << a.el
     << "," << a.ec << ")";
  return os;
}

bool ShortLoc::contains(const ShortLoc& b) const {
  bool cont = model_path == b.model_path
      && (sl < b.sl || (sl == b.sl && sc <= b.sc))
      && (el > b.el || (el == b.el && ec >= b.ec));
  return cont;
}

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
    ExpressionExtractor ee(locs, std::cout);
    iter_items(ee, m);
  } else {
    // std::cerr << "Writing constraint data to: " << output_path << std::endl;
    // std::ofstream out_json_os{output_path};
    // write_data_deps(m, out_json_os);
    // out_json_os.close();
  }

  return e;
}
