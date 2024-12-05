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

namespace MznAnalyse {

Discretise::Discretise(unsigned int base_scale) : base_scale_factor{base_scale} {}

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
  nt.tiExplicit(Expression::type(e).tiExplicit());
  nt.bt(Expression::type(e).bt());
  nt.st(Expression::type(e).st());
  nt.ot(Expression::type(e).ot());
  nt.otExplicit(Expression::type(e).otExplicit());
  nt.cv(Expression::type(e).cv());
  nt.any(Expression::type(e).any());
  nt.typeId(Expression::type(e).typeId());
  nt.dim(Expression::type(e).dim());
  Expression::type(e, nt);
}

inline Expression* scaleDown(Env* env, unsigned int scale, Expression* e) {
  if (scale == 1) return e;

  if (!Expression::isa<Id>(e) && Expression::type(e).ti() == Type::TI_PAR) {
    IntVal iv = eval_int(env->envi(), e);
    return FloatLit::a(iv / scale);
  }

  Expression* bo = new BinOp(Location().introduce(), e, BOT_IDIV, IntLit::a(scale));
  toVar(bo);
  return bo;
}
inline Expression* scaleDownF(Env* env, unsigned int scale, Expression* e) {
  if (scale == 1) return e;
  if (Expression::isa<Id>(e) && Expression::cast<Id>(e)->decl()->ti()->ranges().size() > 0) {
    VarDecl* c = new VarDecl(Location().introduce(),
                             new TypeInst(Location().introduce(), Type::parint()), "c");
    Expression* scaling = scaleDownF(env, scale, c->id());
    Generator gen{{c}, {e}, NULL};
    Generators gens;
    gens.g = {gen};
    Comprehension* cs = new Comprehension(Location().introduce(), scaling, gens, false);

    return cs;
  }
  return new BinOp(Location().introduce(), e, BOT_DIV, IntLit::a(scale));
}

inline Expression* scaleUp(Env* env, unsigned int scale, Expression* e) {
  if (scale == 1) return e;

  if (!Expression::isa<Id>(e) && Expression::type(e).ti() == Type::TI_PAR) {
    if (Expression::type(e).bt() == Type::BT_FLOAT) {
      FloatVal fv = eval_float(env->envi(), e);
      return FloatLit::a(scale * fv);
    } else {
      IntVal iv = eval_int(env->envi(), e);
      return IntLit::a(scale * iv);
    }
  }

  BinOp* bo = new BinOp(Location().introduce(), IntLit::a(scale), BOT_MULT, e);
  toVar(bo);

  return bo;
}

inline Expression* roundE(Env* env, Expression* e) {
  if (!Expression::isa<Id>(e) && Expression::type(e).ti() == Type::TI_PAR) {
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
  for (unsigned int i = 0; i < al->size(); i++) {
    Expression* e = (*al)[i];
    if (Id* id = Expression::dynamicCast<Id>(e)) {
      new_arr.push_back(id);
    } else {
      if (FloatLit* fl = Expression::dynamicCast<FloatLit>(e)) {
        new_arr.push_back(scaleAndRound(env, scale, fl));
      } else {
        new_arr.push_back(scaleUp(env, scale, e));
      }
    }
  }
  return new ArrayLit(Location().introduce(), new_arr);
}

Expression* floatArr2IntArr(Env* env, unsigned int base_scale_factor, Expression* a) {
  // Generator approach

  ArrayLit* cs = eval_array_lit(env->envi(), a);
  vector<Expression*> new_a;

  for (unsigned int i = 0; i < cs->size(); i++) {
    Expression* e = (*cs)[i];

    if (FloatLit* fl = Expression::dynamicCast<FloatLit>(e)) {
      new_a.push_back(scaleAndRound(env, base_scale_factor, fl));
    } else {
      new_a.push_back(scaleUp(env, base_scale_factor, (*cs)[i]));
    }
  }

  return new ArrayLit(Location().introduce(), new_a);
}

Call* process_lin_eq_defines(Env* e, unsigned int base_scale_factor, Call* ca, Id* defined_id) {
  ArrayLit* cs = eval_array_lit(e->envi(), ca->arg(0));
  ArrayLit* vs = varFloatArr2varIntArr(e, base_scale_factor, eval_array_lit(e->envi(), ca->arg(1)));
  Expression* b = scaleAndRound(e, base_scale_factor * base_scale_factor, ca->arg(2));

  vector<Expression*> new_cs;
  for (unsigned int i = 0; i < vs->size(); i++) {
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
  copyAnns(Expression::ann(ca), Expression::ann(nc));
  return nc;
}

Call* process_lin(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo,
                  Call* ca, std::string name) {
  // Coefficients
  Expression* cs = ca->arg(0);

  if (!Expression::isa<Id>(cs)) {
    cs = floatArr2IntArr(e, base_scale_factor, cs);
  }

  // Variables
  Expression* vs =
      varFloatArr2varIntArr(e, base_scale_factor, eval_array_lit(e->envi(), ca->arg(1)));

  // Bound
  Expression* b = scaleAndRound(e, base_scale_factor * base_scale_factor, ca->arg(2));

  Call* nc = Call::a(Location().introduce(), name, {cs, vs, b});
  copyAnns(Expression::ann(ca), Expression::ann(nc));
  return nc;
}

Call* process_lin_eq(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo,
                     Call* ca) {
  return process_lin(e, base_scale_factor, varinfo, ca, "int_lin_eq");
}

Call* process_lin_le(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo,
                     Call* ca) {
  return process_lin(e, base_scale_factor, varinfo, ca, "int_lin_le");
}

Call* process_binop(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo,
                    Call* ca, std::string name) {
  Expression* lhs_expr = ca->arg(0);
  unsigned int lhs_scale = base_scale_factor;

  if (Id* id = Expression::dynamicCast<Id>(lhs_expr)) {
    auto lhsvarinfo = varinfo.find(id);
    if (lhsvarinfo != varinfo.end()) {
      lhs_scale = lhsvarinfo->second->s;
    }
  }
  Expression* lhs = scaleAndRound(e, lhs_scale, lhs_expr);

  Expression* rhs_expr = ca->arg(1);
  unsigned int rhs_scale = base_scale_factor;

  if (Id* id = Expression::dynamicCast<Id>(rhs_expr)) {
    auto rhsvarinfo = varinfo.find(id);
    if (rhsvarinfo != varinfo.end()) {
      rhs_scale = rhsvarinfo->second->s;
    }
  }
  Expression* rhs = scaleAndRound(e, rhs_scale, rhs_expr);

  Call* nc = Call::a(Location().introduce(), name, {lhs, rhs});
  copyAnns(Expression::ann(ca), Expression::ann(nc));
  return nc;
}

Call* process_eq(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo,
                 Call* ca) {
  return process_binop(e, base_scale_factor, varinfo, ca, "int_eq");
}

Call* process_le(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo,
                 Call* ca) {
  return process_binop(e, base_scale_factor, varinfo, ca, "int_le");
}

Expression* process_int2float(Env* e, unsigned int base_scale_factor,
                              unordered_map<Id*, VarInfo*>& varinfo, Call* ca) {
  Call* eq = Call::a(Location().introduce(), "int_eq", {ca->arg(0), ca->arg(1)});
  copyAnns(Expression::ann(ca), Expression::ann(eq));
  return eq;
}

Expression* makeIntCon(unsigned int base_scale_factor, VarDecl* vd) {
  BinOp* mod = new BinOp(Location().introduce(), vd->id(), BOT_MOD, IntLit::a(base_scale_factor));
  BinOp* integer = new BinOp(Location().introduce(), mod, BOT_EQ, IntLit::a(0));

  return integer;
}

Expression* process(Env* e, unsigned int base_scale_factor, unordered_map<Id*, VarInfo*>& varinfo,
                    Call* ca) {
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

inline bool isVar(Expression* vd) { return Expression::type(vd).isvar(); };
inline bool isFloat(Expression* vd) { return Expression::type(vd).bt() == Type::BT_FLOAT; }
inline bool isInt(Expression* vd) { return Expression::type(vd).bt() == Type::BT_INT; }
inline bool isArray(VarDecl* vd) { return vd->ti()->ranges().size() > 0; }

VarDecl* process(Env* e, VarInfo* vinfo) {
  VarDecl* vd = vinfo->vd;

  // std::cout << "Original: " << *vd << "\n";

  if (!isVar(vd)) {
    // replace rhs with rounded version
    if (isArray(vd)) {
      if (isFloat(vd)) {
        // change float to int in type
        vd->ti(new TypeInst(Location().introduce(), Type::parint(), vd->ti()->ranges()));
        // Replace rhs with rounded version
        vd->e(floatArr2IntArr(e, vinfo->s, vd->e()));
      } else {
        vd->e(floatArr2IntArr(e, vinfo->s, vd->e()));
      }
    } else {
      if (isFloat(vd)) {
        vd->ti(new TypeInst(Location().introduce(), Type::parint(), vd->ti()->ranges()));
        vd->e(scaleAndRound(e, vinfo->s, vd->e()));
      } else {
        vd->e(scaleUp(e, vinfo->s, vd->e()));
      }
    }
  } else {
    if (!isArray(vd)) {
      if (isFloat(vd)) {
        // change float to int in type
        TypeInst* ti = vd->ti();
        // In FlatZinc we can assume contiguous domain
        Expression* domain = ti->domain();
        Expression* new_domain = nullptr;

        if (domain) {
          if (SetLit* sl = Expression::dynamicCast<SetLit>(domain)) {
            FloatSetVal* fsv = sl->fsv();
            FloatVal lb = fsv->min();
            FloatVal ub = fsv->max();
            Expression* l = scaleAndRound(e, vinfo->s, FloatLit::a(lb));
            Expression* u = scaleAndRound(e, vinfo->s, FloatLit::a(ub));
            new_domain = new BinOp(Location().introduce(), l, BOT_DOTDOT, u);
          } else if (BinOp* bo = Expression::dynamicCast<BinOp>(domain)) {
            Expression* l = scaleAndRound(e, vinfo->s, bo->lhs());
            Expression* u = scaleAndRound(e, vinfo->s, bo->rhs());
            new_domain = new BinOp(Location().introduce(), l, BOT_DOTDOT, u);
          }
        }
        TypeInst* nti =
            new TypeInst(Location().introduce(), Type::varint(), vd->ti()->ranges(), new_domain);
        vd->ti(nti);
      } else {
        // change float to int in type
        TypeInst* ti = vd->ti();
        // In FlatZinc we can assume contiguous domain
        Expression* domain = ti->domain();
        Expression* new_domain = nullptr;

        if (domain) {
          if (SetLit* sl = Expression::dynamicCast<SetLit>(domain)) {
            IntSetVal* isv = sl->isv();
            IntVal lb = isv->min();
            IntVal ub = isv->max();
            Expression* l = scaleUp(e, vinfo->s, IntLit::a(lb));
            Expression* u = scaleUp(e, vinfo->s, IntLit::a(ub));
            new_domain = new BinOp(Location().introduce(), l, BOT_DOTDOT, u);
          } else if (BinOp* bo = Expression::dynamicCast<BinOp>(domain)) {
            Expression* l = scaleUp(e, vinfo->s, bo->lhs());
            Expression* u = scaleUp(e, vinfo->s, bo->rhs());
            new_domain = new BinOp(Location().introduce(), l, BOT_DOTDOT, u);
          }
        }
        TypeInst* nti =
            new TypeInst(Location().introduce(), Type::varint(), vd->ti()->ranges(), new_domain);
        vd->ti(nti);
      }
    } else {
      // change float to int in type
      vd->ti(new TypeInst(Location().introduce(), Type::varint(), vd->ti()->ranges()));
      // scale any constants in the rhs (in flatzinc there should always be a rhs)
      if (vd->e()) {
        if (ArrayLit* al = Expression::dynamicCast<ArrayLit>(vd->e())) {
          vd->e(varFloatArr2varIntArr(e, vinfo->s, al));
        }
      }
    }
  }

  return vd;
}

MiniZinc::Env* Discretise::run(MiniZinc::Env* e, std::ostream& log) {
  Model* m = e->model();

  // Collect variables and build map: id -> scaling factor
  unordered_map<Id*, VarInfo*> varinfo;
  vector<VarInfo*> outputs;
  vector<VarInfo*> integers;
  for (VarDeclI& vdi : m->vardecls()) {
    VarDecl* vd = vdi.e();
    VarInfo* vdinfo = new VarInfo(vd, base_scale_factor);
    varinfo[vdi.e()->id()] = vdinfo;

    if (isVar(vd) && isInt(vd) && !isArray(vd)) {
      integers.push_back(vdinfo);
    }

    // Collect which variables have an ouput annotation so we can build
    //   FlatZinc output model later
    if (Expression::ann(vd).containsCall(Constants::constants().ann.output_array.aststr()) ||
        Expression::ann(vd).contains(Constants::constants().ann.output_var)) {
      outputs.push_back(vdinfo);
      Expression::addAnnotation(vd, Constants::constants().ann.add_to_output);
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
    if (Call* ca = Expression::dynamicCast<Call>(ci.e())) {
      if (ca->id() == "float_lin_eq") {
        if (Call* defines_var =
                Expression::ann(ca).getCall(Constants::constants().ann.defines_var)) {
          condef.push_back(&ci);
          continue;
        }
      }
    }

    conrest.push_back(&ci);
  }

  for (ConstraintI* ci : condef) {
    Call* ca = Expression::dynamicCast<Call>(ci->e());
    Call* dv = Expression::ann(ca).getCall(Constants::constants().ann.defines_var);
    Id* di = Expression::cast<Id>(dv->arg(0));
    ci->e(process_lin_eq_defines(e, base_scale_factor, ca, di->decl()->id()));
  }

  for (ConstraintI* ci : conrest) {
    Call* ca = Expression::dynamicCast<Call>(ci->e());
    ci->e(process(e, base_scale_factor, varinfo, ca));
  }

  // Add constraints for integer variables
  for (auto vinfo : integers) {
    if (base_scale_factor > 1) {
      ConstraintI* ci =
          new ConstraintI(Location().introduce(), makeIntCon(base_scale_factor, vinfo->nvd));
      m->addItem(ci);
    }
  }

  vector<Expression*> output_strings;
  for (auto vi : outputs) {
    stringstream ss;
    ss << vi->vd->id()->str() << " = ";

    vector<Expression*> args;
    args.push_back(new StringLit(Location().introduce(), ss.str()));
    args.push_back(Call::a(Location().introduce(), "format",
                           {scaleDownF(e, base_scale_factor, vi->vd->id())}));
    args.push_back(new StringLit(Location().introduce(), ";\n"));
    ArrayLit* al = new ArrayLit(Location().introduce(), args);
    output_strings.push_back(Call::a(Location().introduce(), "concat", {al}));
  }

  OutputI* oi =
      new OutputI(Location().introduce(), new ArrayLit(Location().introduce(), output_strings));
  m->addItem(oi);
  m->setOutputItem(oi);

  // Cleanup
  for (auto id_vdi : varinfo) {
    delete id_vdi.second;
  }

  return e;
}

}  // namespace MznAnalyse
