#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>

#include <minizinc/model.hh>
#include <minizinc/copy.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/solver.hh>
#include <minizinc/astiterator.hh>
#include <minizinc/prettyprinter.hh>

using namespace MiniZinc;
using std::string;
using std::vector;


FunctionI* construct_data_ann(EnvI& envi) {
  vector<VarDecl*> params;
  params.push_back(
      new VarDecl(
        Location().introduce(),
        new TypeInst(Location().introduce(), Type::parint()),
        "depth"));
  params.push_back(
      new VarDecl(
        Location().introduce(),
        new TypeInst(Location().introduce(), Type::parstring()),
        "name"));
  params.push_back(
      new VarDecl(
        Location().introduce(),
        new TypeInst(Location().introduce(), Type::parstring()),
        "value"));
  TypeInst* ti = new TypeInst(Location().introduce(), Type::ann());

  return new FunctionI(
      Location().introduce(),
      "data",
      ti,
      params);
}


Expression* data(EnvI& envi, size_t depth, Expression* e) {
  std::stringstream ss;
  ss << *e;

  Expression* e_copy = copy(envi, e);
  e_copy->ann().clear();

  vector<Expression*> args;
  args.push_back(IntLit::a(depth));
  args.push_back(new StringLit(Location().introduce(), ss.str()));
  args.push_back(new Call(Location().introduce(), "show", {e_copy}));
  Call* ca = new Call(Location().introduce(), "data", args);
  ca->type(Type::ann());
  return ca;
}

typedef std::unordered_set<Expression*, ExpressionHash, ExpressionEq> ExprSet;

struct Scope {
  ExprSet par_deps;
  Expression* parent;

  Scope(Expression* p) : parent{p} {}
  Scope(Scope& s, Expression* p) : par_deps{s.par_deps}, parent{p} {}
};

struct Frame {
  size_t depth;
  Expression* e;

  Frame(size_t d, Expression* expr) : depth{d}, e{expr} {}
};

/// Push all elements of \a v onto \a stack
template <class E>
void pushVec(size_t depth, std::vector<Frame>& stack, ASTExprVec<E> v) {
  for (unsigned int i = 0; i < v.size(); i++) {
    stack.emplace_back(depth+1,v[i]);
  }
}

void annotateWithData(EnvI& envi, Expression* root, bool verbose = false) {
  //vector<Scope> scopes;
  //scopes.emplace_back(nullptr);
  //

  std::vector<Frame> stack;
  stack.emplace_back(0, root);

  while (!stack.empty()) {
    Frame f = stack.back();
    size_t depth = f.depth;
    Expression* e = f.e;
    stack.pop_back();
    if (e == nullptr) {
      continue;
    }

    //if(e->type().isvarbool()) {
    //  // ANNOTATE

    //  if(!scopes.back().par_deps.empty()) {
    //    std::vector<Expression*> concat_strings;
    //    concat_strings.push_back(new StringLit(Location().introduce(), "{"));

    //    int i = scopes.back().par_deps.size();
    //    for(Expression* e : scopes.back().par_deps) {
    //      std::stringstream ss;
    //      ss << "\"" << *e << "\": \"";
    //      concat_strings.push_back(new StringLit(Location().introduce(), ss.str()));
    //      std::vector<Expression*> e_vec;
    //      e_vec.push_back(e);
    //      concat_strings.push_back(new Call(Location().introduce(), "show", e_vec));

    //      if(--i) {
    //        concat_strings.push_back(new StringLit(Location().introduce(), "\","));
    //      } else {
    //        concat_strings.push_back(new StringLit(Location().introduce(), "\""));
    //      }
    //    }

    //    concat_strings.push_back(new StringLit(Location().introduce(), "}"));

    //    ArrayLit* concat_stringlit = new ArrayLit(Location().introduce(), concat_strings);

    //    std::vector<Expression*> call_args = { concat_stringlit };
    //    std::vector<Expression*> args = { new Call(Location().introduce(), "concat", call_args) };

    //    Call* ca = new Call(e->loc(), "mzn_expression_name", args);
    //    ca->type(Type::ann());
    //    e->addAnnotation(ca);
    //  }

    //} else if(scopes.back().parent != e && e->type().isPar() && !e->isa<VarDecl>()) {
    //  // COLLECT
    //  if(e->isa<Id>()) {
    //    scopes.back().par_deps.insert(e->cast<Id>()->decl()->id());
    //    if(verbose)
    //      std::cerr << "    COLLECTING: " << *e << std::endl;
    //  } else {
    //    if(scopes.back().par_deps.find(e) == scopes.back().par_deps.end()) {
    //      scopes.back().par_deps.insert(e);
    //      if(verbose)
    //        std::cerr << "    COLLECTING: " << *e << std::endl;
    //    }
    //  }
    //}

    switch (e->eid()) {
      case Expression::E_INTLIT:
        break;
      case Expression::E_FLOATLIT:
        break;
      case Expression::E_SETLIT:
        pushVec(depth+1, stack, e->template cast<SetLit>()->v());
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
          stack.emplace_back(depth+1, (*e->cast<ArrayLit>())[i]);
        }
        break;
      case Expression::E_ARRAYACCESS:
        pushVec(depth+1, stack, e->template cast<ArrayAccess>()->idx());
        stack.emplace_back(depth+1, e->template cast<ArrayAccess>()->v());
        break;
      case Expression::E_COMP:
        {
          auto* comp = e->template cast<Comprehension>();
          stack.emplace_back(depth+1, comp->e());
          //if(verbose)
          //  std::cerr << "NEW SCOPE comp: " << *e << std::endl;
          //scopes.emplace_back(scopes.back(), comp->e());

          for (unsigned int i = comp->numberOfGenerators(); (i--) != 0U;) {
            if(comp->e()->type().isvarbool() && comp->where(i) && comp->where(i)->type().isPar()) {
              comp->e()->addAnnotation(data(envi, depth, comp->where(i)));
            }
            if(comp->e()->type().isvarbool() && comp->in(i)->type().isPar()) {
              comp->e()->addAnnotation(data(envi, depth, comp->in(i)));
            }
            stack.emplace_back(depth+1,comp->where(i));
            stack.emplace_back(depth+1,comp->in(i));
            for (unsigned int j = comp->numberOfDecls(i); (j--) != 0U;) {
              if(comp->e()->type().isvarbool() && comp->in(i)->type().isPar()) {
                comp->e()->addAnnotation(data(envi, depth, comp->decl(i, j)->id()));
              }
              stack.emplace_back(depth+1,comp->decl(i, j));
            }
          }
        }
        break;
      case Expression::E_ITE:
        {
          ITE* ite = e->template cast<ITE>();
          stack.emplace_back(depth+1,ite->elseExpr());
          //scopes.emplace_back(scopes.back(), ite->elseExpr());
          //for (size_t j = 0; j < ite->size(); j++) {
          //  scopes.back().par_deps.insert(
          //    new UnOp(
          //      Location().introduce(),
          //      UOT_NOT,
          //      ite->ifExpr(j)));
          //}
          for (size_t j = 0; j < ite->size(); j++) {
            if(ite->elseExpr()->type().isvarbool() && ite->ifExpr(j)->type().isPar()) {
              ite->elseExpr()->addAnnotation(
                  data(envi, depth,
                    new UnOp(
                      Location().introduce(),
                      UOT_NOT,
                      ite->ifExpr(j))));
            }
          }


          for (size_t i = 0; i < ite->size(); i++) {
            stack.emplace_back(depth+1,ite->thenExpr(i));
            stack.emplace_back(depth+1,ite->ifExpr(i));
            //scopes.emplace_back(scopes.back(), ite->thenExpr(i));
            //for (size_t j = 0; j < i; j++) {
            //  scopes.back().par_deps.insert(
            //    new UnOp(
            //      Location().introduce(),
            //      UOT_NOT,
            //      ite->ifExpr(j)));
            //}
            //scopes.back().par_deps.insert(ite->ifExpr(i));
            for (size_t j = 0; j < i; j++) {
              if(ite->thenExpr(i)->type().isvarbool() && ite->ifExpr(j)->type().isPar()) {
                ite->thenExpr(i)->addAnnotation(
                    data(envi,
                      depth,
                      new UnOp(
                        Location().introduce(),
                        UOT_NOT,
                        ite->ifExpr(j))));
              }
            }
            if(ite->thenExpr(i)->type().isvarbool() && ite->ifExpr(i)->type().isPar()) {
              ite->thenExpr(i)->addAnnotation(
                  data(envi, depth, ite->ifExpr(i)));
            }
          }
        }
        break;
      case Expression::E_BINOP:
        stack.emplace_back(depth+1,e->template cast<BinOp>()->rhs());
        stack.emplace_back(depth+1,e->template cast<BinOp>()->lhs());
        break;
      case Expression::E_UNOP:
        stack.emplace_back(depth+1,e->template cast<UnOp>()->e());
        break;
      case Expression::E_CALL:
        for (unsigned int i = 0; i < e->template cast<Call>()->argCount(); i++) {
          stack.emplace_back(depth+1,e->template cast<Call>()->arg(i));
        }
        break;
      case Expression::E_VARDECL:
        stack.emplace_back(depth+1,e->template cast<VarDecl>()->e());
        break;
      case Expression::E_LET:
        // Have to find solution for this
        stack.emplace_back(depth+1,e->template cast<Let>()->in());
        pushVec(depth+1, stack, e->template cast<Let>()->let());
        break;
      case Expression::E_TI:
        stack.emplace_back(depth+1,e->template cast<TypeInst>()->domain());
        pushVec(depth+1, stack, e->template cast<TypeInst>()->ranges());
        break;
      case Expression::E_TIID:
        break;
    }

    //if(scopes.back().parent == e) {
    //  if(verbose)
    //    std::cerr << "EXITING SCOPE: " << *e << std::endl;
    //  scopes.pop_back();
    //}
  }
}

int main(int argc, char**argv) {
  GCLock lock;
  bool verbose = false;
  if(argc < 2) {
    std::cerr << argv[0] << ": Invalid arguments." << std::endl;
    std::cerr << "Usage: " << argv[0] << " <mzn>" << std::endl;
    return EXIT_FAILURE;
  }
  vector<string> mzn_paths;
  for(int i=1; i<argc; i++) {
    if(string(argv[i]) == "-v") {
      verbose = true;
    } else {
      mzn_paths.push_back(argv[i]);
    }
  }

  vector<string> includes;
  string mzn_stdlib_dir = FileUtils::share_directory();
  includes.push_back(mzn_stdlib_dir + "/std/");

  Env env;
  Model* m = parse(env, mzn_paths, {}, "", "", includes, false, false, false, false, std::cerr);
  vector<TypeError> typeErrors;
  try {
    typecheck(env, m, typeErrors, true, true, true);
  } catch (TypeError &e) {
    typeErrors.push_back(e);
  }
  if (typeErrors.size() > 0) {
    for (unsigned int i = 0; i < typeErrors.size(); i++) {
      std::cerr << typeErrors[i].loc() << ":" << std::endl;
      std::cerr << typeErrors[i].what() << ":" << typeErrors[i].msg()
        << std::endl;
    }
    exit(EXIT_FAILURE);
  }

  if(!m) {
    std::cerr << argv[0] << ": Failed to parse file" << std::endl;
    return EXIT_FAILURE;
  }

  for(ConstraintI& ci : m->constraints()) {
    annotateWithData(env.envi(), ci.e(), verbose);
    if(verbose)
      std::cerr << std::endl;
  }
  m->addItem(construct_data_ann(env.envi()));

  Printer pp(std::cout, 80);
  pp.print(m);

  return EXIT_SUCCESS;
}
