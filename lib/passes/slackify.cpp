#include "passes/slackify.hh"

#include <fstream>
#include <iostream>
#include <minizinc/astiterator.hh>
#include <minizinc/eval_par.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/flatten.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>
#include <minizinc/type.hh>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "string_utils.hh"

using namespace MiniZinc;
using std::ostream;
using std::string;
using std::unordered_map;
using std::vector;

namespace MznAnalyse {

struct SlackParInfo {
  ASTString basename;    // Original name of parameter
  Id* id;                // Identifier of slackified version
  Ref<Expression> e;     // RHS of original vardecl
  Ref<Expression> down;  // Down slack
  Ref<Expression> up;    // Up slack

  SlackParInfo(ASTString bname, Id* orig_id, Expression* rhs, Expression* d, Expression* u)
      : basename{bname}, id{orig_id}, e{rhs}, down{d}, up{u} {}
};

Slackify::Slackify() {}

// Force decls of Ids to use the correct Id
struct IdReplacer : public MiniZinc::EVisitor {
  bool enter(MiniZinc::Expression* e) { return e; };
  void vId(Id* id) {
    if (VarDecl* vd = id->decl()) {
      id->v(vd->id()->v());
    }
  }
};

// Walk expression and check if any slack_pars are used
// Use ignore_decl to avoid self check
struct IdFinder : public MiniZinc::EVisitor {
  bool found = false;
  const vector<SlackParInfo>& slack_pars;
  const VarDecl* ignore_decl;

  IdFinder(const vector<SlackParInfo>& sps, const VarDecl* idecl)
      : slack_pars{sps}, ignore_decl{idecl} {}

  bool enter(MiniZinc::Expression* e) { return !found && e != nullptr; };
  void vId(Id* id) {
    for (const SlackParInfo& spi : slack_pars) {
      if (spi.id->decl() != ignore_decl && id->decl() == spi.id->decl()) {
        found = true;
        return;
      }
    }
  }
};

// Check if any slack vars are used by this VarDeclI
// Return whether slack is used in the domain, rhs, or not used
enum SlackUse { S_NONE, S_DOM, S_EXPR };
SlackUse testSlackUsage(const vector<SlackParInfo>& slack_pars, VarDeclI& vdi) {
  VarDecl* vd = vdi.e();
  IdFinder idf(slack_pars, vd);

  top_down(idf, vd->ti()->domain());
  if (idf.found) return S_DOM;

  top_down(idf, vd->e());
  if (idf.found) return S_EXPR;

  return S_NONE;
}

// Take array of expressions and combine them into 1d array
// If expression is not an array, wrap in array lit "[e]"
// If expression is an array, join it with '++'
// array_concat([a1 = x, a2 = [y,z]]) => [a1] ++ array1d(a2)
Ref<Expression> array_concat(const vector<Ref<Expression>>& arrays, int i = 0) {
  if (i >= arrays.size()) return nullptr;

  Ref<Expression> this_array = arrays[i];
  Type ty = Expression::type(this_array);
  if (ty.dim() == 0) {
    vector<Expression*> args = {this_array};
    this_array = make<ArrayLit>(Location().introduce(), args);
  } else if (ty.dim() > 1) {
    vector<Expression*> args = {this_array};
    this_array = Call::a(Location().introduce(), "array1d", args);
  }

  if (i == arrays.size() - 1) return this_array;
  return make<BinOp>(Location().introduce(), this_array, BinOpType::BOT_PLUSPLUS,
                     array_concat(arrays, i + 1));
}

MiniZinc::Env* Slackify::run(MiniZinc::Env* e, std::ostream& log) {
  Model* m = e->model();

  // Collect vars that have "slack_par" annotation
  vector<SlackParInfo> slack_pars;
  ASTString slack_par("slack_par");
  for (VarDeclI& vdi : m->vardecls()) {
    VarDecl* vd = vdi.e();
    Annotation& ann = Expression::ann(vd);
    if (Call* ca = ann.getCall(slack_par)) {
      slack_pars.emplace_back(vd->id()->str(), vd->id(), vd->e(), ca->arg(0), ca->arg(1));
      ann.removeCall(slack_par);
    }
  }

  // If there are no slack variables do not perform translation
  // TODO: This is probably incorrect. The user is expecting a particular interface
  //       including old_objective_lb/ub/eq options so maybe we should still transform
  //       the model.
  if (slack_pars.empty()) {
    return e;
  }

  // include slacks_internal.mzn;
  m->addItem(make<IncludeI>(Location().introduce(), ASTString("slacks_internal.mzn")));

  // Rename existing Ids and their declarations and force par -> var
  for (const SlackParInfo& spi : slack_pars) {
    std::stringstream ss;
    ss << spi.basename << "_slack";
    std::string name = ss.str();

    spi.id->v(ASTString(name));

    VarDecl* psvd = spi.id->decl();

    psvd->id()->v(ASTString(name));
    psvd->ti()->mkVar(e->envi());

    // Clear the rhs of the var version of the par
    psvd->e(nullptr);

    // Make sure this version is part of output
    Expression::addAnnotation(psvd, MiniZinc::Constants().ann.output);
  }

  // Handle special cases where slackified par (now var) is used
  vector<Ref<Item>> items_to_add;
  for (VarDeclI& vdi : m->vardecls()) {
    SlackUse su = testSlackUsage(slack_pars, vdi);

    if (su == S_DOM) {
      // MiniZinc does not support var set of int as domain
      // Rewrite 'var 1..n: x;' => 'var int: x; constraint x in 1..n;'
      VarDecl* vd = vdi.e();
      if (Expression* domain = vd->ti()->domain()) {
        vector<Expression*> args;
        args.push_back(vd->id()), args.push_back(domain);
        items_to_add.push_back(make<ConstraintI>(
            Expression::loc(domain), Call::a(Expression::loc(domain), "slack_domain", args)));
        vd->ti()->domain(nullptr);
      }
    } else if (su == S_EXPR) {
      // If rhs of decl references slackified par (now var)
      // Rewrite 'int: p = n + 5;' => 'var int: p = n + 5;'
      VarDecl* vd = vdi.e();
      vd->ti()->mkVar(e->envi());
    }
  }
  for (const Ref<Item>& ii : items_to_add) {
    m->addItem(ii);
  }

  // Update as many identifiers as possible (not all are correctly linked to the vardecl)
  IdReplacer id_replacer;
  for (VarDeclI& vdi : m->vardecls()) {
    top_down(id_replacer, vdi.e());
  }
  for (ConstraintI& ci : m->constraints()) {
    top_down(id_replacer, ci.e());
  }
  if (SolveI* si = m->solveItem()) {
    if (Expression* obj = si->e()) {
      top_down(id_replacer, obj);
    }
  }

  // Add new vars, re-introduce pars with old names
  vector<Ref<Expression>> all_slacks;
  for (const SlackParInfo& spi : slack_pars) {
    VarDecl* vd = Expression::dynamicCast<VarDecl>(spi.id->decl());

    // Re-introduce par with original name
    Ref<Id> newbaseid = make<Id>(Expression::loc(spi.id), spi.basename, nullptr);
    Ref<TypeInst> newTi = copy(e->envi(), vd->ti()).cast<TypeInst>();
    newTi->mkPar(e->envi());

    Ref<VarDecl> newBasePar = make<VarDecl>(Expression::loc(spi.id), newTi, newbaseid, nullptr);
    newBasePar->e(spi.e);

    m->addItem(VarDeclI::a(Expression::loc(spi.id), newBasePar));

    // Add slack_up version
    std::stringstream upss;
    upss << spi.basename << "_slack_up";
    std::string upname = upss.str();
    Ref<Id> newupid = make<Id>(Expression::loc(spi.id), upname, nullptr);

    Ref<TypeInst> upti = copy(e->envi(), vd->ti()).cast<TypeInst>();
    upti->domain(make<BinOp>(Location().introduce(), IntLit::a(0), BOT_DOTDOT, spi.up));

    Ref<VarDecl> newSlackUp = make<VarDecl>(Expression::loc(spi.id), upti, newupid, nullptr);

    m->addItem(VarDeclI::a(Expression::loc(spi.id), newSlackUp));

    all_slacks.push_back(newupid);

    // Add slack_down version
    std::stringstream downss;
    downss << spi.basename << "_slack_down";
    std::string downname = downss.str();
    Ref<Id> newdownid = make<Id>(Expression::loc(spi.id), downname, nullptr);

    Ref<TypeInst> downti = copy(e->envi(), vd->ti()).cast<TypeInst>();
    downti->domain(make<BinOp>(Location().introduce(), IntLit::a(0), BOT_DOTDOT, spi.down));

    Ref<VarDecl> newSlackDown = make<VarDecl>(Expression::loc(spi.id), downti, newdownid, nullptr);

    m->addItem(VarDeclI::a(Expression::loc(spi.id), newSlackDown));
    all_slacks.push_back(newdownid);

    // Link slackified var with up and down slacks
    vector<Expression*> args;
    args.push_back(newbaseid);
    args.push_back(newdownid);
    args.push_back(newupid);
    args.push_back(spi.down);
    args.push_back(spi.up);
    spi.id->decl()->e(Call::a(Expression::loc(spi.id), ASTString("slack_link"), args));
  }

  // Assign all_slacks array for use in objective
  m->addItem(
      make<AssignI>(Location().introduce(), ASTString("all_slacks"), array_concat(all_slacks)));

  // Call slack_configure_objective() predicate to set up slack objective
  m->addItem(make<ConstraintI>(
      Location().introduce(),
      Call::a(Location().introduce(), ASTString("slack_configure_objective"), {})));

  // Process objective function
  SolveI* si = m->solveItem();

  // If there is no solve item add an empty minimize one
  if (!si) {
    Ref<SolveI> new_si = SolveI::min(Location().introduce(), nullptr);
    m->addItem(new_si);
    si = new_si;
  }

  // Store existing objective function
  // If the problem was a satisfaction problem, fix objective to 0;
  Ref<Expression> obj_expr = si->e();
  if (obj_expr == nullptr) {
    obj_expr = IntLit::a(0);
  }

  // Set up link between objective function and "old_objective" var
  Ref<Id> so_id = make<Id>(Location().introduce(), ASTString("old_objective"), nullptr);
  m->addItem(make<ConstraintI>(Location().introduce(),
                               make<BinOp>(Location().introduce(), so_id, BOT_EQ, obj_expr)));

  // Make sure solve type is minimize, and add "slack_objective" var as objective var;
  si->st(SolveI::SolveType::ST_MIN);
  si->e(make<Id>(Location().introduce(), ASTString("slack_objective"), nullptr));

  // Remove output item
  if (OutputI* oi = m->outputItem()) {
    oi->remove();
  }

  return e;
}

};  // namespace MznAnalyse
