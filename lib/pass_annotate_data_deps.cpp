#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "pass_annotate_data_deps.hh"

#include <minizinc/astiterator.hh>
#include <minizinc/copy.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>

using namespace MiniZinc;
using std::string;
using std::stringstream;
using std::unordered_map;
using std::vector;

FunctionI *construct_data_ann(int nargs) {
  vector<VarDecl *> params;
  // int: depth
  params.push_back(new VarDecl(
      Location().introduce(),
      new TypeInst(Location().introduce(), Type::parint()), "depth"));
  // string: type
  params.push_back(new VarDecl(
      Location().introduce(),
      new TypeInst(Location().introduce(), Type::parstring()), "name"));
  // list of string: args
  for (int i = 0; i < nargs; i++) {
    std::stringstream ss;
    ss << "arg_" << i << "_";
    params.push_back(new VarDecl(
        Location().introduce(),
        new TypeInst(Location().introduce(), Type::parstring()), ss.str()));
  }
  TypeInst *ti = new TypeInst(Location().introduce(), Type::ann());

  return new FunctionI(Location().introduce(), string("data"), ti, params);
}
FunctionI *construct_data_term_ann() {
  vector<VarDecl *> params;
  // int: depth
  params.push_back(new VarDecl(
      Location().introduce(),
      new TypeInst(Location().introduce(), Type::parint()), "depth"));

  // string: type
  params.push_back(new VarDecl(
      Location().introduce(),
      new TypeInst(Location().introduce(), Type::parstring()), "name"));

  // list of string: generators
  {
    vector<TypeInst *> t_idx = {
        new TypeInst(Location().introduce(), Type::parint())};
    TypeInst *ti = new TypeInst(Location().introduce(), Type::parstring());
    ti->setRanges(t_idx);
    params.push_back(new VarDecl(Location().introduce(), ti, "gens"));
  }

  // string: location
  params.push_back(new VarDecl(
      Location().introduce(),
      new TypeInst(Location().introduce(), Type::parstring()), "locs"));

  // list of string: coefs
  {
    vector<TypeInst *> t_idx = {
        new TypeInst(Location().introduce(), Type::parint())};
    TypeInst *ti = new TypeInst(Location().introduce(), Type::parstring());
    ti->setRanges(t_idx);
    params.push_back(new VarDecl(Location().introduce(), ti, "coefs"));
  }

  // string: variable
  params.push_back(new VarDecl(
      Location().introduce(),
      new TypeInst(Location().introduce(), Type::parstring()), "variable"));

  TypeInst *ti = new TypeInst(Location().introduce(), Type::ann());

  return new FunctionI(Location().introduce(), string("data"), ti, params);
}

Expression *without_anns(EnvI &envi, Expression *e) {
  Expression *e_copy = copy(envi, e);
  e_copy->ann().clear();
  return e_copy;
}

Expression *toStringLit(EnvI &envi, Expression *e) {
  std::stringstream ss;
  ss << *(without_anns(envi, e));
  return new StringLit(Location().introduce(), ss.str());
}

Expression *toShow(EnvI &envi, Expression *e) {
  return new Call(Location().introduce(), "show", {without_anns(envi, e)});
}

Expression *data_ann(EnvI &envi, size_t depth, string type,
                     vector<Expression *> exprs) {
  // Construct data(string: type, int: depth, list of string: args);
  vector<Expression *> args;
  args.push_back(IntLit::a(depth));
  args.push_back(new StringLit(Location().introduce(), type));
  args.insert(args.end(), exprs.begin(), exprs.end());
  Call *ca = new Call(Location().introduce(), "data", args);
  ca->type(Type::ann());
  return ca;
}

Expression *data_eq(EnvI &envi, size_t depth, Expression *e) {
  if (e->isa<IntLit>() || e->isa<BoolLit>() || e->isa<SetLit>() ||
      e->isa<ArrayLit>() || e->isa<StringLit>()) {
    return data_ann(envi, depth, "lit", {toShow(envi, e)});
  }
  return data_ann(envi, depth, "eq", {toStringLit(envi, e), toShow(envi, e)});
}
Expression *data_assign(EnvI &envi, size_t depth, Expression *e) {
  if (e->isa<IntLit>() || e->isa<BoolLit>() || e->isa<SetLit>() ||
      e->isa<ArrayLit>() || e->isa<StringLit>()) {
    return data_ann(envi, depth, "lit", {toShow(envi, e)});
  }
  return data_ann(envi, depth, "assign",
                  {toStringLit(envi, e), toShow(envi, e)});
}

Expression *data_in(EnvI &envi, size_t depth, Expression *id, Expression *in) {
  return data_ann(envi, depth, "in",
                  {toStringLit(envi, id), toStringLit(envi, in)});
}

Expression *data_if(EnvI &envi, size_t depth, Expression *where) {
  return data_ann(envi, depth, "if", {toStringLit(envi, where)});
}

// Expression* data_if_exists(EnvI& envi, size_t depth, Expression* e) {
//  Call* exists = e->cast<Call>();
//  Comprehension* comp = exists->args(0)->dyn_cast<Comprehension*>();
//
//  // exists(i in A, j in B where i < j) ( p[i,j] ) ->
//  //
//  // let {
//  //   int: tmp_i = arg_max(i in A) (exists(j in B where i < j) ( p[i,j] );
//  //   int: tmp_j = arg_max(j in B where tmp_i < j) ( p[tmp_i, j] )
//  // } in join(", ", ["i=" ++ show(A[tmp_i]), "j=" ++ show(B[tmp_j])]);
//
//  vector<Expression*> vars(comp->numberOfGenerators());
//  vector<Expression*> string_exps(2*comp->numberOfGenerators());
//
//  for (unsigned int i = comp->numberOfGenerators(); (i--) != 0U;) {
//    for (unsigned int j = comp->numberOfDecls(i); (j--) != 0U;) {
//      VarDecl* new_var = new VarDecl(Location().introduce(),
//          "tmp" + comp->);
//  }
//
//  Expression* in = new Call(Location().introduce(), "concat", string_exps);
//  Let* let = new Let(envi, vars, in);
//  std::cerr << "Exists let: " << *exists << "\n======\n" << *let << "\n";
//
//  exit(EXIT_FAILURE);
//
//  return data_ann(envi, depth, "if_exists", { toStringLit(envi, where) });
//}

struct Frame {
  size_t depth;
  Expression *e;

  Frame(size_t d, Expression *expr) : depth{d}, e{expr} {}
};

/// Push all elements of \a v onto \a stack
template <class E>
void pushVec(size_t depth, std::vector<Frame> &stack, ASTExprVec<E> v) {
  for (unsigned int i = 0; i < v.size(); i++) {
    stack.emplace_back(depth + 1, v[i]);
  }
}

void annotateWithData(EnvI &envi, Expression *root) {
  std::vector<Frame> stack;
  stack.emplace_back(0, root);

  while (!stack.empty()) {
    Frame f = stack.back();
    size_t depth = f.depth;
    Expression *e = f.e;
    stack.pop_back();
    if (e == nullptr) {
      continue;
    }

    switch (e->eid()) {
    case Expression::E_INTLIT:
      break;
    case Expression::E_FLOATLIT:
      break;
    case Expression::E_SETLIT:
      pushVec(depth + 1, stack, e->template cast<SetLit>()->v());
      break;
    case Expression::E_BOOLLIT:
      break;
    case Expression::E_STRINGLIT:
      break;
    case Expression::E_ID:
      break;
    case Expression::E_ANON:
      break;
    case Expression::E_ARRAYLIT:
      for (unsigned int i = 0; i < e->cast<ArrayLit>()->size(); i++) {
        stack.emplace_back(depth + 1, (*e->cast<ArrayLit>())[i]);
      }
      break;
    case Expression::E_ARRAYACCESS:
      pushVec(depth + 1, stack, e->template cast<ArrayAccess>()->idx());
      stack.emplace_back(depth + 1, e->template cast<ArrayAccess>()->v());
      break;
    case Expression::E_COMP: {
      auto *comp = e->template cast<Comprehension>();
      stack.emplace_back(depth + 1, comp->e());

      for (unsigned int i = comp->numberOfGenerators(); (i--) != 0U;) {
        if (comp->e()->type().isvarbool() && comp->where(i) &&
            comp->where(i)->type().isPar()) {
          comp->e()->addAnnotation(data_if(envi, depth, comp->where(i)));
        }
        if (comp->e()->type().isvarbool() && comp->in(i)->type().isPar()) {
          comp->e()->addAnnotation(data_eq(envi, depth, comp->in(i)));
        }
        stack.emplace_back(depth + 1, comp->where(i));
        stack.emplace_back(depth + 1, comp->in(i));
        for (unsigned int j = comp->numberOfDecls(i); (j--) != 0U;) {
          if (comp->e()->type().isvarbool()) {
            if (comp->decl(i, j)->type().isPar()) {
              comp->e()->addAnnotation(
                  data_assign(envi, depth, comp->decl(i, j)->id()));
              if (comp->in(i)->type().isPar()) {
                comp->e()->addAnnotation(
                    data_in(envi, depth, comp->decl(i, j)->id(), comp->in(i)));
              }
            }
          }
          stack.emplace_back(depth + 1, comp->decl(i, j));
        }
      }
    } break;
    case Expression::E_ITE: {
      ITE *ite = e->template cast<ITE>();
      stack.emplace_back(depth + 1, ite->elseExpr());
      for (size_t j = 0; j < ite->size(); j++) {
        if (ite->elseExpr()->type().isvarbool() &&
            ite->ifExpr(j)->type().isPar()) {
          ite->elseExpr()->addAnnotation(data_if(
              envi, depth,
              new UnOp(Location().introduce(), UOT_NOT, ite->ifExpr(j))));
        }
      }

      for (size_t i = 0; i < ite->size(); i++) {
        stack.emplace_back(depth + 1, ite->thenExpr(i));
        stack.emplace_back(depth + 1, ite->ifExpr(i));
        for (size_t j = 0; j < i; j++) {
          if (ite->thenExpr(i)->type().isvarbool() &&
              ite->ifExpr(j)->type().isPar()) {
            ite->thenExpr(i)->addAnnotation(data_if(
                envi, depth,
                new UnOp(Location().introduce(), UOT_NOT, ite->ifExpr(j))));
          }
        }
        if (ite->thenExpr(i)->type().isvarbool() &&
            ite->ifExpr(i)->type().isPar()) {
          Expression *if_expr = ite->ifExpr(i);
          // Special behaviour for exists
          // if(if_expr->isa<Call>() && if_expr->cast<Call>()->id() == "exists")
          // {
          //  data_if_exists(envi, depth, if_expr);
          //}
          // Copy the condition directly
          ite->thenExpr(i)->addAnnotation(data_if(envi, depth, if_expr));
        }
      }
    } break;
    case Expression::E_BINOP:
      // Collect functional assignments
      {
        BinOp *bo = e->template cast<BinOp>();
        stack.emplace_back(depth + 1, bo->rhs());
        stack.emplace_back(depth + 1, bo->lhs());
      }
      break;
    case Expression::E_UNOP:
      stack.emplace_back(depth + 1, e->template cast<UnOp>()->e());
      break;
    case Expression::E_CALL:
      for (unsigned int i = 0; i < e->template cast<Call>()->argCount(); i++) {
        stack.emplace_back(depth + 1, e->template cast<Call>()->arg(i));
      }
      break;
    case Expression::E_VARDECL:
      stack.emplace_back(depth + 1, e->template cast<VarDecl>()->e());
      break;
    case Expression::E_LET:
      // Have to find solution for this
      stack.emplace_back(depth + 1, e->template cast<Let>()->in());
      pushVec(depth + 1, stack, e->template cast<Let>()->let());
      break;
    case Expression::E_TI:
      stack.emplace_back(depth + 1, e->template cast<TypeInst>()->domain());
      pushVec(depth + 1, stack, e->template cast<TypeInst>()->ranges());
      break;
    case Expression::E_TIID:
      break;
    }
  }
}

AnnotateDataDeps::AnnotateDataDeps() {}

MiniZinc::Env* AnnotateDataDeps::run(MiniZinc::Env* e, std::ostream& log) {
  // Add data annotations
  Model* m = e->model();
  for (ConstraintI &ci : m->constraints()) {
    annotateWithData(e->envi(), ci.e());
  }
  m->addItem(construct_data_ann(1));
  m->addItem(construct_data_ann(2));
  m->compact();

  return e;
}
