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
  ASTString basename;
  Id* id;
  Expression* e;
  Expression* down;
  Expression* up;

  SlackParInfo(ASTString bname, Id* orig_id, Expression* rhs, Expression* d, Expression* u)
      : basename{bname}, id{orig_id}, e{rhs}, down{d}, up{u} {}
};

Slackify::Slackify() {}

std::string Slackify::get_name() { return "slackify"; }

struct IdReplacer : public MiniZinc::EVisitor {
  bool enter(MiniZinc::Expression* e) { return e; };
  void vId(Id* id) { 
    if(VarDecl* vd = id->decl()) {
      id->v(vd->id()->v());
    }
  }
};

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

bool hasSlackVarDomain(const vector<SlackParInfo>& slack_pars, VarDeclI& vdi) {
  VarDecl* vd = vdi.e();
  IdFinder idf(slack_pars, vd);
  top_down(idf, vd->ti()->domain());
  return idf.found;
}

Expression* array_concat(const vector<Expression*>& arrays, int i = 0) {
  if (i >= arrays.size()) return nullptr;

  Expression* this_array = arrays[i];
  Type ty = Expression::type(this_array);
  if (ty.dim() == 0) {
    vector<Expression*> args = {this_array};
    this_array = new ArrayLit(Location().introduce(), args);
  } else if (ty.dim() > 1) {
    vector<Expression*> args = {this_array};
    this_array = Call::a(Location().introduce(), "array1d", args);
  }

  if (i == arrays.size() - 1) return this_array;
  return new BinOp(Location().introduce(), this_array, BinOpType::BOT_PLUSPLUS,
                   array_concat(arrays, i + 1));
}

MiniZinc::Env* Slackify::run(MiniZinc::Env* e, std::ostream& log) {
  Model* m = e->model();

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

  if (slack_pars.empty()) {
    return e;
  }

  m->addItem(new IncludeI(Location().introduce(), ASTString("slacks_internal.mzn")));

  for (const SlackParInfo& spi : slack_pars) {
    std::stringstream ss;
    ss << spi.basename << "_slack";
    std::string name = ss.str();

    spi.id->v(ASTString(name));

    VarDecl* psvd = spi.id->decl();

    psvd->id()->v(ASTString(name));
    psvd->ti()->mkVar(e->envi());
    psvd->e(nullptr);

    Expression::addAnnotation(psvd, MiniZinc::Constants().ann.output);
  }

  // Move variable domains out
  vector<Item*> items_to_add;
  for (VarDeclI& vdi : m->vardecls()) {
    if (hasSlackVarDomain(slack_pars, vdi)) {
      VarDecl* vd = vdi.e();

      if (Expression* domain = vd->ti()->domain()) {
        vector<Expression*> args;
        args.push_back(vd->id()), args.push_back(domain);
        Expression* x_in_dom = Call::a(Expression::loc(domain), "slack_domain", args);
        ConstraintI* ci = new ConstraintI(Expression::loc(domain), x_in_dom);
        items_to_add.push_back(ci);
        vd->ti()->domain(nullptr);
      }
    }
  }
  for (Item* ii : items_to_add) {
    m->addItem(ii);
  }

  IdReplacer id_replacer;
  for (VarDeclI& vdi : m->vardecls()) {
    top_down(id_replacer, vdi.e());
  }
  for (ConstraintI& ci : m->constraints()) {
    top_down(id_replacer, ci.e());
  }
  if(SolveI* si = m->solveItem()) {
    if(Expression* obj = si->e()) {
      top_down(id_replacer, obj);
    }
  }

  vector<Expression*> all_slacks;

  for (const SlackParInfo& spi : slack_pars) {
    // Add new vars
    VarDecl* vd = Expression::dynamicCast<VarDecl>(spi.id->decl());

    Id* newbaseid = new Id(Expression::loc(spi.id), spi.basename, nullptr);
    TypeInst* newTi = Expression::dynamicCast<TypeInst>(copy(e->envi(), vd->ti()));
    newTi->mkPar(e->envi());

    VarDecl* newBasePar = new VarDecl(Expression::loc(spi.id), newTi, newbaseid, nullptr);
    newBasePar->e(spi.e);

    VarDeclI* basevdi = VarDeclI::a(Expression::loc(spi.id), newBasePar);
    m->addItem(basevdi);

    std::stringstream upss;
    upss << spi.basename << "_slack_up";
    std::string upname = upss.str();
    Id* newupid = new Id(Expression::loc(spi.id), upname, nullptr);

    TypeInst* upti = Expression::dynamicCast<TypeInst>(copy(e->envi(), vd->ti()));
    upti->domain(new BinOp(Location().introduce(), IntLit::a(0), BOT_DOTDOT, spi.up));

    VarDecl* newSlackUp = new VarDecl(Expression::loc(spi.id), upti, newupid, nullptr);

    VarDeclI* upvdi = VarDeclI::a(Expression::loc(spi.id), newSlackUp);
    m->addItem(upvdi);

    all_slacks.push_back(newupid);

    std::stringstream downss;
    downss << spi.basename << "_slack_down";
    std::string downname = downss.str();
    Id* newdownid = new Id(Expression::loc(spi.id), downname, nullptr);

    TypeInst* downti = Expression::dynamicCast<TypeInst>(copy(e->envi(), vd->ti()));
    downti->domain(new BinOp(Location().introduce(), IntLit::a(0), BOT_DOTDOT, spi.down));

    VarDecl* newSlackDown = new VarDecl(Expression::loc(spi.id), downti, newdownid, nullptr);

    VarDeclI* downvdi = VarDeclI::a(Expression::loc(spi.id), newSlackDown);
    m->addItem(downvdi);
    all_slacks.push_back(newdownid);

    vector<Expression*> args;
    args.push_back(newbaseid);
    args.push_back(newdownid);
    args.push_back(newupid);
    args.push_back(spi.down);
    args.push_back(spi.up);
    Call* ca = Call::a(Expression::loc(spi.id), ASTString("slack_link"), args);
    spi.id->decl()->e(ca);
  }

  Expression* all_slacks_array = array_concat(all_slacks);
  AssignI* all_slacks_assign = new AssignI(
    Location().introduce(),
    ASTString("all_slacks"),
    all_slacks_array
  );
  m->addItem(all_slacks_assign);

  Call* obj_call = Call::a(Location().introduce(), ASTString("slack_configure_objective"), {});
  ConstraintI* sco_ci = new ConstraintI(Location().introduce(), obj_call);
  m->addItem(sco_ci);

  SolveI* si = m->solveItem();

  if (!si) {
    si = SolveI::min(Location().introduce(), nullptr);
    m->addItem(si);
  }

  Expression* obj_expr = si->e();
  if (!obj_expr) {
    obj_expr = IntLit::a(0);
  }

  Id* so_id = new Id(Location().introduce(), ASTString("old_objective"), nullptr);
  BinOp* bo = new BinOp(Location().introduce(), so_id, BOT_EQ, obj_expr);
  ConstraintI* ci = new ConstraintI(Location().introduce(), bo);
  m->addItem(ci);

  si->st(SolveI::SolveType::ST_MIN);
  si->e(new Id(Location().introduce(), ASTString("slack_objective"), nullptr));

  if (OutputI* oi = m->outputItem()) {
    oi->remove();
  }

  return e;
}

};  // namespace MznAnalyse
