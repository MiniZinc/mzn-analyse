#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>

#include <minizinc/model.hh>
#include <minizinc/copy.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/solver.hh>
#include <minizinc/astiterator.hh>
#include <minizinc/prettyprinter.hh>

using namespace MiniZinc;
using std::string;
using std::stringstream;
using std::vector;
using std::unordered_map;

void buildAnnotation(EnvI& envi, vector<Expression*>& generators,
                     vector<Expression*>& coefs, Expression* var) {
  vector<Expression*> args;
  args.push_back(new ArrayLit(Location().introduce(), generators));

  const string minor_sep = "|";

  Location loc = var->loc();
  {
    std::stringstream ss;
    ss << loc.filename() << minor_sep << loc.firstLine() << minor_sep << loc.firstColumn()
      << minor_sep << loc.lastLine() << minor_sep << loc.lastColumn() << minor_sep;
    args.push_back(new StringLit(Location().introduce(), ss.str()));
  }

  args.push_back(new ArrayLit(Location().introduce(), coefs));
  {
    stringstream ss;
    ss << *var;
    args.push_back(new StringLit(Location().introduce(), ss.str()));
  }

  std::cerr
    << "TERM: Location:   " << *args[1] << "\n"
    << "      Generators: " << *args[0] << "\n"
    << "      Coefs:      " << *args[2] << "\n"
    << "      Variable:   " << *args[3] << "\n";
}

struct StackFrame {
  size_t gen_idx;
  size_t coef_idx;
  Expression* e;

  StackFrame(size_t g, size_t c, Expression* exp)
    : gen_idx{ g }, coef_idx{ c }, e{ exp } {}
};

void addTermAnnotations(EnvI& envi, SolveI* si,
  unordered_map<Id*, Expression*>& assigns, Expression* root) {
  // For now just support:
  // sum(gens where clauses) (coef1 * var1 + coef2 * var2)
  vector<Expression*> gens;
  vector<Expression*> coefs;

  vector<StackFrame> stack;
  stack.emplace_back(0,0,root);

  while (!stack.empty()) {
    StackFrame frame = stack.back();
    while (gens.size() > frame.gen_idx) {
      gens.pop_back();
    }
    while (coefs.size() > frame.coef_idx) {
      coefs.pop_back();
    }
    stack.pop_back();

    if (Call* call = frame.e->dynamicCast<Call>()) {
      if (call->id() == "sum") {
        Comprehension* co = call->arg(0)->cast<Comprehension>();
        // collect generators
        for (size_t i = 0; i < co->numberOfGenerators(); i++) {
          Expression* in = co->in(i);
          for (size_t j = 0; j < co->numberOfDecls(i); j++) {
            stringstream ss;
            VarDecl* idx = co->decl(i, j);
            ss << *idx->id() << " in " << *in;
            gens.push_back(new StringLit(Location().introduce(), ss.str()));
          }
        }
        stack.emplace_back(gens.size(), coefs.size(), co->e());
      } else {
        buildAnnotation(envi, gens, coefs, call);
      }
    } else if (BinOp* bo = frame.e->dynamicCast<BinOp>()) {
      if (bo->op() == BOT_MULT) {
        if (bo->lhs()->type().isPar()) {
          stringstream ss;
          ss << *bo->lhs();
          coefs.push_back(new StringLit(Location().introduce(), ss.str()));
          stack.emplace_back(gens.size(), coefs.size(), bo->rhs());
        } else {
          stringstream ss;
          ss << *bo->rhs();
          coefs.push_back(new StringLit(Location().introduce(), ss.str()));
          stack.emplace_back(gens.size(), coefs.size(), bo->lhs());
        }
      } else if (bo->op() == BOT_PLUS) {
        if (bo->lhs()->type().isvar()) {
          stack.emplace_back(gens.size(), coefs.size(), bo->lhs());
        }
        if (bo->rhs()->type().isvar()) {
          stack.emplace_back(gens.size(), coefs.size(), bo->rhs());
        }
      } else {
        std::cerr << "UNHANDLED BinOp type" << std::endl;
        exit(EXIT_FAILURE);
      }
    } else if (Id* id = frame.e->dynamicCast<Id>()) {
      auto it = assigns.find(id->decl()->id());
      if (it != assigns.end()) {
        stack.emplace_back(gens.size(), coefs.size(), it->second);
      } else {
        // It is just an ID
        coefs.push_back(new StringLit(Location().introduce(), id->str()));
        buildAnnotation(envi, gens, coefs, id);
      }
    } else {
      buildAnnotation(envi, gens, coefs, frame.e);
    }
  }
}

void annotateObjective(EnvI& envi, SolveI* si,
                       unordered_map<Id*,Expression*>& assigns) {
  Expression* obj_e = si->e();
  if(!obj_e) {
    std::cerr << "No objective function" << std::endl;
    return;
  }

  Expression* e = obj_e;
  while(Id* id = e->dynamicCast<Id>()) {
    e = id->decl()->e();
    if(!e) {
      auto it = assigns.find(id->decl()->id());
      if(it != assigns.end()) {
        e = it->second;
      }
      if(!e) return;
    }
  }
  if(!e) return;

  addTermAnnotations(envi, si, assigns, e);
}

namespace MznData {
  void objective(std::vector<std::string>& mzn_paths) {
    GCLock lock;
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
      std::cerr << "data objective: Failed to parse file" << std::endl;
      return;
    }

    // Collect functional assignments for objective processing
    unordered_map<Id*, Expression*> assigns;

    // Add data annotations
    for(ConstraintI& ci : m->constraints()) {
      if(BinOp* bo = ci.e()->dynamicCast<BinOp>()) {
        if (bo->op() == BOT_EQ) {
          if (Id* lhe = bo->lhs()->dynamicCast<Id>()) {
            assigns[lhe->decl()->id()] = bo->rhs();
          }
          if (Id* rhe = bo->rhs()->dynamicCast<Id>()) {
            assigns[rhe->decl()->id()] = bo->lhs();
          }
        }
      }
    }

    // Add coef annotations to objective terms
    annotateObjective(env.envi(), m->solveItem(), assigns);
    m->solveItem()->remove();
    m->outputItem()->remove();
    m->compact();

    // Write annotated model to stdout
    Printer pp(std::cout, 80, false);
    pp.print(m);
  }
};
