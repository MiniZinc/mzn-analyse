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

void toVar(Expression* e) {
  Type nt;
  nt.ti(Type::TI_VAR);
  nt.tiExplicit(e->type().tiExplicit());
  nt.bt(e->type().bt());
  nt.st(e->type().st());
  nt.ot(e->type().ot());
  nt.otExplicit(e->type().otExplicit());
  nt.cv(e->type().cv());
  nt.any(e->type().any());
  nt.typeId(e->type().typeId());
  nt.dim(e->type().dim());
  e->type(nt);
}

inline Expression* scaleDown(Env* env, unsigned int scale, Expression* e) {
  if (scale == 1) return e;

  if(!e->isa<Id>() && e->type().ti() == Type::TI_PAR) {
    IntVal iv = eval_int(env->envi(), e);
    return FloatLit::a(iv / scale);
  }

  Expression* bo = new BinOp(Location().introduce(), e, BOT_IDIV, IntLit::a(scale));
  toVar(bo);
  return bo;
}
inline Expression* scaleDownF(Env* env, unsigned int scale, Expression* e) {
  if (scale == 1) return e;
  if (e->isa<Id>() && e->cast<Id>()->decl()->ti()->ranges().size() > 0) {
    VarDecl* c = new VarDecl(Location().introduce(), new TypeInst(Location().introduce(), Type::parint()), "c");
    Expression* scaling = scaleDownF(env, scale, c->id());
    Generator gen {{c}, {e}, NULL};
    Generators gens;
    gens.g = {gen};
    Comprehension* cs = new Comprehension(Location().introduce(), scaling, gens, false);

    return cs;
  }
  return new BinOp(Location().introduce(), e, BOT_DIV, IntLit::a(scale));
}

inline Expression* scaleUp(Env* env, unsigned int scale, Expression* e) {
  if (scale == 1) return e;

  if (!e->isa<Id>() && e->type().ti() == Type::TI_PAR) {
    FloatVal fv = eval_float(env->envi(), e);
    return FloatLit::a(scale * fv);
  }

  BinOp* bo = new BinOp(Location().introduce(), IntLit::a(scale), BOT_MULT, e);
  toVar(bo);

  return bo;
}

inline Expression* roundE(Env* env, Expression* e) {
  if(!e->isa<Id>() && e->type().ti() == Type::TI_PAR) {
    FloatVal fv = eval_float(env->envi(), e);
    return IntLit::a(round(fv.toDouble()));
  }
  Call* ca = Call::a(Location().introduce(), "round", {e});
  toVar(ca);
  return ca;
}

Expression* scaleAndRound(Env* env, unsigned int scale, Expression* e) {
  return roundE(env, scaleUp(env, scale, e));
}

void copyAnns(Annotation& aa, Annotation& ab) {
  for (ExpressionSetIter it = aa.begin(); it != aa.end(); ++it) {
    ab.add(*it);
  }
}

ArrayLit* varFloatArr2varIntArr(Env* env, unsigned int scale, ArrayLit* al) {
  vector<Expression*> new_arr;
  for (unsigned int i = 0; i<al->size(); i++) {
    Expression* e = (*al)[i];
    if (Id* id = e->dynamicCast<Id>()) {
      new_arr.push_back(id);
    } else {
      new_arr.push_back(scaleAndRound(env, scale, e));
    }
  }
  return new ArrayLit(Location().introduce(), new_arr);
}

Expression* floatArr2IntArr(Env* env, unsigned int base_scale_factor, Expression* a) {
  // Generator approach
  
  ArrayLit* cs = eval_array_lit(env->envi(), a);
  vector<Expression*> new_a;

  for(unsigned int i=0; i<cs->size(); i++) {
    new_a.push_back(scaleAndRound(env, base_scale_factor, (*cs)[i]));
  }

  return new ArrayLit(Location().introduce(), new_a);
}

Call* process_lin_eq_defines(Env* e, unsigned int base_scale_factor, Call* ca, Id* defined_id) {
  ArrayLit* cs = eval_array_lit(e->envi(), ca->arg(0));
  ArrayLit* vs = varFloatArr2varIntArr(e, base_scale_factor, eval_array_lit(e->envi(), ca->arg(1)));
  Expression* b = scaleAndRound(e, base_scale_factor*base_scale_factor, ca->arg(2));

  vector<Expression*> new_cs;
  for(unsigned int i = 0; i < vs->size(); i++) {
    Expression* v = (*vs)[i];
    Expression* c = (*cs)[i];

    // if (v->isa<Id>() && v->cast<Id>()->decl()->id() == defined_id) {
    //   // No scaling
    //   new_cs.push_back(scaleAndRound(e, 1, c));
    // } else {
      new_cs.push_back(scaleAndRound(e, base_scale_factor, c));
    //}
  }

  ArrayLit* new_cs_al = new ArrayLit(Location().introduce(), new_cs);

  Call* nc = Call::a(Location().introduce(), "int_lin_eq", {new_cs_al, vs, b});
  copyAnns(ca->ann(), nc->ann());
  return nc;
}

Call* process_lin(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca, std::string name) {
  // Coefficients
  Expression* cs = ca->arg(0);

  if (!cs->isa<Id>() ) {
    cs = floatArr2IntArr(e, base_scale_factor, cs);
  }

  // Variables
  Expression* vs = varFloatArr2varIntArr(e, base_scale_factor, eval_array_lit(e->envi(), ca->arg(1)));

  // Bound
  Expression* b = scaleAndRound(e, base_scale_factor*base_scale_factor, ca->arg(2));

  Call* nc = Call::a(Location().introduce(), name, {cs, vs, b});
  copyAnns(ca->ann(), nc->ann());
  return nc;

}

Call* process_lin_eq(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca) {
  return process_lin(e, base_scale_factor, varinfo, ca, "int_lin_eq");
}

Call* process_lin_le(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca) {
  return process_lin(e, base_scale_factor, varinfo, ca, "int_lin_le");
}

Call* process_binop(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca, std::string name) {
  Expression* lhs_expr = ca->arg(0);
  unsigned int lhs_scale = base_scale_factor;

  if (Id* id = lhs_expr->dynamicCast<Id>()) {
    auto lhsvarinfo = varinfo.find(id);
    if (lhsvarinfo != varinfo.end()) {
      lhs_scale = lhsvarinfo->second->s;
    }
  }
  Expression *lhs = scaleAndRound(e, lhs_scale, lhs_expr);

  Expression* rhs_expr = ca->arg(1);
  unsigned int rhs_scale = base_scale_factor;

  if (Id* id = rhs_expr->dynamicCast<Id>()) {
    auto rhsvarinfo = varinfo.find(id);
    if (rhsvarinfo != varinfo.end()) {
      rhs_scale = rhsvarinfo->second->s;
    }
  }
  Expression *rhs = scaleAndRound(e, rhs_scale, rhs_expr);

  Call* nc = Call::a(Location().introduce(), name, {lhs, rhs});
  copyAnns(ca->ann(), nc->ann());
  return nc;
}

Call* process_eq(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca) {
  return process_binop(e, base_scale_factor, varinfo, ca, "int_eq");
}

Call* process_le(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca) {
  return process_binop(e, base_scale_factor, varinfo, ca, "int_le");
}

Call* process_int2float(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca) {
  Expression* left = ca->arg(0);
  Expression* right = ca->arg(1);

  Call* nc = Call::a(Location().introduce(), "int_eq", {left, scaleDown(e, base_scale_factor, right)});
  copyAnns(ca->ann(), nc->ann());
  return nc;
}

Call* process(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo, Call* ca) {
  if (ca->id() == Constants::constants().ids.float_.lin_eq) {
    return process_lin_eq(e, base_scale_factor, varinfo, ca);
  } else if (ca->id() == Constants::constants().ids.float_.lin_le) {
    return process_lin_le(e, base_scale_factor, varinfo, ca);
  } else if (ca->id() == Constants::constants().ids.float_.eq) {
    return process_eq(e, base_scale_factor, varinfo, ca);
  } else if (ca->id() == Constants::constants().ids.float_.le) {
    return process_le(e, base_scale_factor, varinfo, ca);
  } else if (ca->id() == Constants::constants().ids.int2float) {
    return process_int2float(e, base_scale_factor, varinfo, ca);
  } else {
    return ca;
  }
}

inline bool isVarFloat(VarDecl* vd) { return vd->type().isvar() && vd->type().bt() == Type::BT_FLOAT && vd->ti()->ranges().size() == 0; }
inline bool isVarFloatArray(VarDecl* vd) { return vd->type().isvar() && vd->type().bt() == Type::BT_FLOAT && vd->ti()->ranges().size() > 0; }
inline bool isParFloat(VarDecl* vd) { return !vd->type().isvar() && vd->type().bt() == Type::BT_FLOAT && vd->ti()->ranges().size() == 0; }
inline bool isParFloatArray(VarDecl* vd) { return !vd->type().isvar() && vd->type().bt() == Type::BT_FLOAT && vd->ti()->ranges().size() > 0; }

VarDecl* process(Env* e, VarInfo* vinfo) {
  VarDecl* vd = vinfo->vd;

  //std::cout << "Original: " << *vd << "\n";

  if (isParFloat(vd)) {
    // change float to int in type
    vd->ti(new TypeInst(Location().introduce(), Type::parint(), vd->ti()->ranges()));
    // replace rhs with rounded version
    vd->e(scaleAndRound(e, vinfo->s, vd->e()));
  } else if (isParFloatArray(vd)) {
    // change float to int in type
    vd->ti(new TypeInst(Location().introduce(), Type::parint(), vd->ti()->ranges()));
    // Replace rhs with rounded version
    vd->e(floatArr2IntArr(e, vinfo->s, vd->e()));
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
        Expression* l = scaleAndRound(e, vinfo->s, FloatLit::a(lb));
        Expression* u = scaleAndRound(e, vinfo->s, FloatLit::a(ub));
        new_domain = new BinOp(Location().introduce(), l, BOT_DOTDOT, u);
      } else if (BinOp* bo = domain->dynamicCast<BinOp>()) {
        Expression* l = scaleAndRound(e, vinfo->s, bo->lhs());
        Expression* u = scaleAndRound(e, vinfo->s, bo->rhs());
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
        vd->e(varFloatArr2varIntArr(e, vinfo->s, al));
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
      vd->addAnnotation(Constants::constants().ann.add_to_output);
    }
  }

  // Create new variables
  for (auto vi : varinfo) {
    VarInfo* vinfo = vi.second;
    vinfo->nvd = process(e, vinfo);
  }

  // Sort constraints so constraints with ::defines_var() annotations
  //   are processed first, as these will probably change the scaling
  //   factor for the defined var.
  vector<ConstraintI*> condef;
  vector<ConstraintI*> conrest;
  vector<Expression*> other;
  for (ConstraintI& ci : m->constraints()) {
    if (Call* ca = ci.e()->dynamicCast<Call>()) {
      if (ca->id() == "float_lin_eq") {
        if (Call* defines_var = ca->ann().getCall(Constants::constants().ann.defines_var)) {
          condef.push_back(&ci);
          continue;
        }
      }
    }

    conrest.push_back(&ci);
  }

  for (ConstraintI* ci : condef) {
    Call* ca = ci->e()->dynamicCast<Call>();
    Call* dv = ca->ann().getCall(Constants::constants().ann.defines_var);
    Id* di = dv->arg(0)->cast<Id>();
    ci->e(process_lin_eq_defines(e, base_scale_factor, ca, di->decl()->id()));
  }

  for (ConstraintI* ci : conrest) {
    Call* ca = ci->e()->dynamicCast<Call>();
    ci->e(process(e, base_scale_factor, varinfo, ca));
  }

  vector<Expression*> output_strings;
  for (auto vi : outputs) {
    stringstream ss;
    ss << vi->vd->id()->str() << " = ";

    vector<Expression*> args;
    args.push_back(new StringLit(Location().introduce(), ss.str()));
    args.push_back(Call::a(Location().introduce(), "format", { scaleDownF(e, base_scale_factor, vi->vd->id()) }));
    args.push_back(new StringLit(Location().introduce(), ";\n"));
    ArrayLit* al = new ArrayLit(Location().introduce(), args);
    output_strings.push_back(Call::a(Location().introduce(), "concat", {al}));
  }

  OutputI* oi = new OutputI(Location().introduce(), new ArrayLit(Location().introduce(), output_strings));
  m->addItem(oi);
  m->setOutputItem(oi);

  // Cleanup
  for(auto id_vdi : varinfo) {
    delete id_vdi.second;
  }

  return e;
}

}  // namespace MznTool
