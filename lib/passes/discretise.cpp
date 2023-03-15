#include "passes/discretise.hh"

#include <fstream>
#include <iostream>
#include <minizinc/astiterator.hh>
#include <minizinc/copy.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "string_utils.hh"

using namespace MiniZinc;
using std::string;
using std::stringstream;
using std::unordered_map;
using std::vector;

namespace MznTool {

Discretise::Discretise(unsigned int base_scale) : base_scale_factor{base_scale} {}

std::string Discretise::get_name() { return "discretise"; }

// void Discretise::write_json(std::ostream& os) { os << json_output; }

struct VarInfo {
  VarDecl* vd;
  VarDecl* nvd;
  unsigned int s;
  VarInfo(VarDecl* vd_, unsigned int scale) : vd{vd_}, nvd{nullptr}, s{scale} {}
};

Expression* scaleAndRound(unsigned int scale, Expression* e) {
  return Call::a(Location().introduce(), "round", {new BinOp(Location().introduce(), IntLit::a(scale), BOT_MULT, e)});
}

ArrayLit* varFloatArr2IntArr(unsigned int scale, ArrayLit* al) {
  vector<Expression*> new_arr;
  for (unsigned int i = 0; i<al->size(); i++) {
    Expression* e = (*al)[i];
    if (Id* id = e->dynamicCast<Id>()) {
      new_arr.push_back(id);
    } else {
      new_arr.push_back(scaleAndRound(scale, e));
    }
  }
  return new ArrayLit(Location().introduce(), new_arr);
}

Comprehension* floatArr2IntArr(unsigned int base_scale_factor, Expression* a) {
  VarDecl* c = new VarDecl(Location().introduce(), new TypeInst(Location().introduce(), Type::parint()), "c");
  Expression* rounding = scaleAndRound(base_scale_factor, c->id());
  Generator gen {{c}, {a}, NULL};
  Generators gens;
  gens.g = {gen};
  Comprehension* cs = new Comprehension(Location().introduce(), rounding, gens, false);
  return cs;
}

Call* process_lin(unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca, std::string name) {
  // Coefficients
  Expression* cs = floatArr2IntArr(base_scale_factor, ca->arg(0));

  // Variables
  Expression* vs = ca->arg(1);

  // Bound
  Expression* b = scaleAndRound(base_scale_factor*base_scale_factor, ca->arg(2));

  return Call::a(Location().introduce(), name, {cs, vs, b});
}

Call* process_lin_eq(unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca) {
  return process_lin(base_scale_factor, varinfo, ca, "int_lin_eq");
}

Call* process_lin_le(unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca) {
  return process_lin(base_scale_factor, varinfo, ca, "int_lin_le");
}

Call* process_binop(unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca, std::string name) {
  Expression* lhs_expr = ca->arg(0);
  unsigned int lhs_scale = base_scale_factor;

  if (Id* id = lhs_expr->dynamicCast<Id>()) {
    auto lhsvarinfo = varinfo.find(id);
    if (lhsvarinfo != varinfo.end()) {
      lhs_scale = lhsvarinfo->second->s;
    }
  }
  Expression *lhs = scaleAndRound(lhs_scale, lhs_expr);

  Expression* rhs_expr = ca->arg(1);
  unsigned int rhs_scale = base_scale_factor;

  if (Id* id = rhs_expr->dynamicCast<Id>()) {
    auto rhsvarinfo = varinfo.find(id);
    if (rhsvarinfo != varinfo.end()) {
      rhs_scale = rhsvarinfo->second->s;
    }
  }
  Expression *rhs = scaleAndRound(rhs_scale, rhs_expr);

  return Call::a(Location().introduce(), name, {lhs, rhs});
}

Call* process_eq(unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca) {
  return process_binop(base_scale_factor, varinfo, ca, "int_eq");
}

Call* process_le(unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca) {
  return process_binop(base_scale_factor, varinfo, ca, "int_le");
}

Call* process(unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca) {
  if (ca->id() == Constants::constants().ids.float_.lin_eq) {
    return process_lin_eq(base_scale_factor, varinfo, ca);
  } else if (ca->id() == Constants::constants().ids.float_.lin_le) {
    return process_lin_le(base_scale_factor, varinfo, ca);
  } else if (ca->id() == Constants::constants().ids.float_.eq) {
    return process_eq(base_scale_factor, varinfo, ca);
  } else if (ca->id() == Constants::constants().ids.float_.le) {
    return process_le(base_scale_factor, varinfo, ca);
  } else {
    return ca;
  }
}

bool isVarFloat(VarDecl* vd) {
  return vd->type().isvar() && vd->type().bt() == Type::BT_FLOAT && vd->ti()->ranges().size() == 0;
}

bool isVarFloatArray(VarDecl* vd) {
  return vd->type().isvar() && vd->type().bt() == Type::BT_FLOAT && vd->ti()->ranges().size() > 0;
}

bool isParFloat(VarDecl* vd) {
  return !vd->type().isvar() && vd->type().bt() == Type::BT_FLOAT && vd->ti()->ranges().size() == 0;
}

bool isParFloatArray(VarDecl* vd) {
  return !vd->type().isvar() && vd->type().bt() == Type::BT_FLOAT && vd->ti()->ranges().size() > 0;
}

VarDecl* process(VarInfo* vinfo) {
  VarDecl* vd = vinfo->vd;

//std::cout << "Original: " << *vd << "\n";

  if (isParFloat(vd)) {
    // change float to int in type
    vd->ti(new TypeInst(Location().introduce(), Type::parint(), vd->ti()->ranges()));
    // replace rhs with rounded version
    vd->e(scaleAndRound(vinfo->s, vd->e()));
  } else if (isParFloatArray(vd)) {
    // change float to int in type
    vd->ti(new TypeInst(Location().introduce(), Type::parint(), vd->ti()->ranges()));
    // Replace rhs with rounded version
    vd->e(floatArr2IntArr(vinfo->s, vd->e()));
  } else if (isVarFloat(vd)) {
    // change float to int in type
    TypeInst* ti = vd->ti();
    // In FlatZinc we can assume contiguous domain
    Expression* domain = ti->domain();
    Expression* new_domain = nullptr;

    if (domain) {
      if (SetLit* sl = domain->dynamicCast<SetLit>()) {
        FloatSetVal* fsv = sl->fsv();
        FloatVal lb = fsv->min();
        FloatVal ub = fsv->max();
        Expression* l = scaleAndRound(vinfo->s, FloatLit::a(lb));
        Expression* u = scaleAndRound(vinfo->s, FloatLit::a(ub));
        new_domain = new BinOp(Location().introduce(), l, BOT_DOTDOT, u);
      } else if (BinOp* bo = domain->dynamicCast<BinOp>()) {
        Expression* l = scaleAndRound(vinfo->s, bo->lhs());
        Expression* u = scaleAndRound(vinfo->s, bo->rhs());
        new_domain = new BinOp(Location().introduce(), l, BOT_DOTDOT, u);
      }
    }
    TypeInst* nti = new TypeInst(Location().introduce(), Type::varint(), vd->ti()->ranges(), new_domain);
    vd->ti(nti);
  } else if (isVarFloatArray(vd)) {
    // change float to int in type
    vd->ti(new TypeInst(Location().introduce(), Type::varint(), vd->ti()->ranges()));
    // scale any constants in the rhs (in flatzinc there should always be a rhs)
    if (vd->e()) {
      if (ArrayLit* al = vd->e()->dynamicCast<ArrayLit>()) {
        vd->e(varFloatArr2IntArr(vinfo->s, al));
      }
    }
  } else {
    //std::cout << "Fell through: " << *vd << "\n";
    return vd;
  }

//  std::cout << "Replacement: " << *vd << "\n";

  return vd;
}

MiniZinc::Env* Discretise::run(MiniZinc::Env* e, std::ostream& log) {
  Model* m = e->model();

  // Collect variables and build map: id -> scaling factor
  unordered_map<Id*, VarInfo*> varinfo;
  vector<VarInfo*> outputs;
  for(VarDeclI& vdi : m->vardecls()) {
    VarDecl* vd = vdi.e();
    VarInfo* vdinfo = new VarInfo(vd, base_scale_factor);
    varinfo[vdi.e()->id()] = vdinfo;

    // Collect which variables have an ouput annotation so we can build
    //   FlatZinc output model later
    if (vd->ann().containsCall(Constants::constants().ann.output_array.aststr()) ||
        vd->ann().contains(Constants::constants().ann.output_var)) {
      outputs.push_back(vdinfo);
    }
  }

  // Create new variables
  for (auto vi : varinfo) {
    VarInfo* vinfo = vi.second;
    vinfo->nvd = process(vinfo);
  }

  // Sort constraints so constraints with ::defines_var() annotations
  //   are processed first, as these will probably change the scaling
  //   factor for the defined var.
  vector<Call*> condef;
  vector<Call*> conrest;
  vector<Expression*> other;
  for (ConstraintI& ci : m->constraints()) {
    if (Call* ca = ci.e()->dynamicCast<Call>()) {
      ci.e(process(base_scale_factor, varinfo, ca));
    }
  }

  // Cleanup
  for(auto id_vdi : varinfo) {
    delete id_vdi.second;
  }

  return e;
}

}  // namespace MznTool
