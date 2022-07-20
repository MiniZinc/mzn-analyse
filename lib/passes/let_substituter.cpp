#include "passes/let_substituter.hh"
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

namespace MznTool {

// Wrap arbitrary Expression* with let
// e -> let {var lb(e)..ub(e): x_i;
//           constraint int_eq(e, x_i);} in x_i
Let *wrap_with_let(Expression *e) {
  VarDecl *vd = nullptr;
  Call *call = nullptr;

  if (e->type().isint()) {
    Call *lb = Call::a(Location().introduce(), "lb", {e});
    Call *ub = Call::a(Location().introduce(), "ub", {e});
    BinOp *bo = new BinOp(Location().introduce(), lb, BOT_DOTDOT, ub);
    TypeInst *ti = new TypeInst(Location().introduce(), Type::varint(), bo);

    vd = new VarDecl(Location().introduce(), ti, "x_i");
    call = Call::a(Location().introduce(), "int_eq", {e, vd->id()});
  } else if (e->type().isfloat()) {
    Call *lb = Call::a(Location().introduce(), "lb", {e});
    Call *ub = Call::a(Location().introduce(), "ub", {e});
    BinOp *bo = new BinOp(Location().introduce(), lb, BOT_DOTDOT, ub);
    TypeInst *ti = new TypeInst(Location().introduce(), Type::varfloat(), bo);

    vd = new VarDecl(Location().introduce(), ti, "x_i");
    call = Call::a(Location().introduce(), "float_eq", {e, vd->id()});
  } else if (e->type().isbool()) {
    TypeInst *ti = new TypeInst(Location().introduce(), Type::varbool());

    vd = new VarDecl(Location().introduce(), ti, "x_i");
    call = Call::a(Location().introduce(), "bool_eq", {e, vd->id()});
  }

  vector<Expression *> context = {vd, call};
  Let *let = new Let(Location().introduce(), context, vd->id());
  return let;
}

// topdown implementation that takes a list of locations and a
// function that produces a Let expression from an expression

template <class T> class TopDownReplacer {
protected:
  /// The visitor to call back during iteration
  T &_t;

  /// Push all elements of \a v onto \a stack
  template <class E>
  static void pushVec(std::vector<Expression *> &stack, ASTExprVec<E> v) {
    for (unsigned int i = 0; i < v.size(); i++) {
      stack.push_back(v[i]);
    }
  }

public:
  /// Constructor
  TopDownReplacer(T &t) : _t(t) {}
  /// Run iterator on expression \a e
  void run(Expression *root);
};

template <class T> void top_down_replace(T &t, Expression *root) {
  TopDownReplacer<T>(t).run(root);
}

template <class T> void TopDownReplacer<T>::run(Expression *root) {
  std::vector<Expression *> stack;
  stack.push_back(root);
  while (!stack.empty()) {
    Expression *e = stack.back();
    stack.pop_back();
    if (e == nullptr) {
      continue;
    }
    for (ExpressionSetIter it = e->ann().begin(); it != e->ann().end(); ++it) {
      // TODO Replace annotations
      stack.push_back(*it);
    }
    switch (e->eid()) {
    case Expression::E_INTLIT:
      break;
    case Expression::E_FLOATLIT:
      break;
    case Expression::E_SETLIT:
      // TODO: Replace set elements
      pushVec(stack, e->template cast<SetLit>()->v());
      break;
    case Expression::E_BOOLLIT:
      break;
    case Expression::E_STRINGLIT:
      break;
    case Expression::E_ID:
      break;
    case Expression::E_ANON:
      break;
    case Expression::E_ARRAYLIT: {
      ArrayLit *al = e->template cast<ArrayLit>();
      for (unsigned int i = 0; i < al->size(); i++) {
        Expression *array_element = (*al)[i];
        if (_t.should_replace(array_element)) {
          al->set(i, _t.get_replacement(array_element));
        } else {
          stack.push_back(array_element);
        }
      }
    } break;
    case Expression::E_ARRAYACCESS: {
      ArrayAccess *aa = e->template cast<ArrayAccess>();
      // TODO pushVec
      pushVec(stack, aa->idx());

      if (_t.should_replace(aa->v())) {
        aa->v(_t.get_replacement(aa->v()));
      } else {
        stack.push_back(e->template cast<ArrayAccess>()->v());
      }
    } break;
    case Expression::E_COMP:
      // TODO: Figure out how to change parts of a comprehension
      {
        auto *comp = e->template cast<Comprehension>();
        for (unsigned int i = comp->numberOfGenerators(); (i--) != 0U;) {
          stack.push_back(comp->where(i));
          stack.push_back(comp->in(i));
          for (unsigned int j = comp->numberOfDecls(i); (j--) != 0U;) {
            stack.push_back(comp->decl(i, j));
          }
        }

        if (_t.should_replace(comp->e())) {
          comp->e(_t.get_replacement(comp->e()));
        } else {
          stack.push_back(comp->e());
        }
      }
      break;
    case Expression::E_ITE: {
      ITE *ite = e->template cast<ITE>();
      if (_t.should_replace(ite->elseExpr())) {
        ite->elseExpr(_t.get_replacement(ite->elseExpr()));
      } else {
        stack.push_back(ite->elseExpr());
      }
      for (int i = 0; i < ite->size(); i++) {
        Expression *ifExpr = ite->ifExpr(i);
        Expression *thenExpr = ite->thenExpr(i);

        // TODO: Not supported
        // if(_t.should_replace(ifExpr)) {
        //  ite->ifExpr(i, _t.get_replacement(ifExpr));
        //} else {
        stack.push_back(ifExpr);
        //}
        if (_t.should_replace(thenExpr)) {
          ite->thenExpr(i, _t.get_replacement(thenExpr));
        } else {
          stack.push_back(thenExpr);
        }
      }
    } break;
    case Expression::E_BINOP: {
      BinOp *bo = e->template cast<BinOp>();
      if (_t.should_replace(bo->rhs())) {
        bo->rhs(_t.get_replacement(bo->rhs()));
      } else {
        stack.push_back(bo->rhs());
      }
      if (_t.should_replace(bo->lhs())) {
        bo->lhs(_t.get_replacement(bo->lhs()));
      } else {
        stack.push_back(bo->lhs());
      }
    } break;
    case Expression::E_UNOP: {
      UnOp *uo = e->template cast<UnOp>();
      if (_t.should_replace(uo->e())) {
        uo->e(_t.get_replacement(uo->e()));
      } else {
        stack.push_back(uo->e());
      }
    } break;
    case Expression::E_CALL: {
      Call *call = e->template cast<Call>();
      for (unsigned int i = 0; i < call->argCount(); i++) {
        if (_t.should_replace(call->arg(i))) {
          call->arg(i, _t.get_replacement(call->arg(i)));
        } else {
          stack.push_back(call->arg(i));
        }
      }
    } break;
    case Expression::E_VARDECL: {
      VarDecl *vd = e->template cast<VarDecl>();
      if (_t.should_replace(vd->e())) {
        vd->e(_t.get_replacement(vd->e()));
      } else {
        stack.push_back(vd->e());
      }
      if (_t.should_replace(vd->ti())) {
        vd->e(_t.get_replacement(vd->ti()));
      } else {
        stack.push_back(vd->ti());
      }
    } break;
    case Expression::E_LET: {
      Let *let = e->template cast<Let>();
      if (_t.should_replace(let->in())) {
        let->in(_t.get_replacement(let->in()));
      } else {
        stack.push_back(let->in());
      }

      // TODO: Deal with pushVec
      pushVec(stack, e->template cast<Let>()->let());
    } break;
    case Expression::E_TI: {
      TypeInst *ti = e->template cast<TypeInst>();
      if (_t.should_replace(ti->domain())) {
        ti->domain(_t.get_replacement(ti->domain()));
      } else {
        stack.push_back(ti->domain());
      }

      // TODO: Deal with pushVec
      pushVec(stack, ti->ranges());
    } break;
    case Expression::E_TIID:
      break;
    }
  }
}

struct ReplacedInfo {
  const Expression *expression;
  const Expression *replacement;

  ReplacedInfo(const Expression *expr, const Expression *rep)
      : expression{expr}, replacement{rep} {}

  ShortLoc loc() const { return ShortLoc{expression->loc()}; }

  std::string to_string() {
    std::string orig_string;
    {
      std::stringstream orig_ss;
      orig_ss << *expression;
      orig_string = orig_ss.str();
    }

    std::string rep_string;
    {
      std::stringstream rep_ss;
      rep_ss << *replacement;
      rep_string = rep_ss.str();
    }

    std::stringstream ss;
    ss << "\n    {\n"
       << "      \"original\": \"" << utils::escape(orig_string) << "\",\n"
       << "      \"replacement\": \"" << utils::escape(rep_string) << "\"\n"
       << "    }";

    return ss.str();
  }
};

struct LetReplacerVisitor {
  std::vector<ReplacedInfo> replacements;
  const std::vector<ShortLoc> &locations;

  LetReplacerVisitor(const std::vector<ShortLoc> &locs) : locations{locs} {}

  bool should_replace(const Expression *e) const {
    bool found_match = false;
    if (e) {
      Location this_loc = e->loc();

      for (const ShortLoc &loc : locations) {
        if (loc == this_loc) {
          found_match = true;
          break;
        }
      }
    }
    return found_match;
  }

  Expression *get_replacement(Expression *e) {
    ShortLoc loc{e->loc()};
    Expression *replacement = wrap_with_let(e);
    replacements.emplace_back(e, replacement);
    return replacement;
  }

  void write_json(ostream &os) {
    std::vector<std::string> entries;

    for (auto &rep : replacements) {
      ShortLoc loc = rep.loc();
      std::stringstream ss;
      ss << "    \"" << loc << "\": " << rep.to_string();
      entries.push_back(ss.str());
    }

    os << "{\n";
    os << utils::join(entries, ",\n", false);
    os << "}" << std::endl;
  }
};

struct ItemExpressionReplacer : public ItemVisitor {
  LetReplacerVisitor lrv;
  ItemExpressionReplacer(const std::vector<ShortLoc> &locs) : lrv{locs} {}

  bool enter(Item *item) { return !item->isa<IncludeI>(); }
  void vVarDeclI(VarDeclI *vdi) {
    VarDecl *vd = vdi->e();
    top_down_replace(lrv, vd);
  }
  void vAssignI(AssignI *ai) {
    top_down_replace(lrv, ai->decl());
    Expression *e = ai->e();
    if (e) {
      if (lrv.should_replace(e)) {
        ai->e(lrv.get_replacement(e));
      } else {
        top_down_replace(lrv, ai->e());
      }
    }
  }
  void vConstraintI(ConstraintI *ci) {
    if (lrv.should_replace(ci->e())) {
      ci->e(lrv.get_replacement(ci->e()));
    } else {
      top_down_replace(lrv, ci->e());
    }
  }
  void vSolveI(SolveI *si) {
    Expression *e = si->e();
    if (e) {
      if (lrv.should_replace(e)) {
        si->e(lrv.get_replacement(e));
      } else {
        top_down_replace(lrv, e);
      }
    }
  }
  void vFunctionI(FunctionI *fi) {}
};

LetSubstituter::LetSubstituter(const std::vector<std::string> &paths) {
  for (const string &path : paths) {
    locs.emplace_back(path);
  }
}

std::string LetSubstituter::get_name() { return "replace-with-newvar"; }
void LetSubstituter::write_json(std::ostream &os) { os << json_output; }

MiniZinc::Env *LetSubstituter::run(MiniZinc::Env *e, std::ostream &log) {
  Model *m = e->model();

  ItemExpressionReplacer ee{locs};
  iter_items(ee, m);

  std::stringstream ss;
  ee.lrv.write_json(ss);
  json_output = ss.str();

  return e;
}
} // namespace MznTool
