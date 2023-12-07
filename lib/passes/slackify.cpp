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
  Expression* range;

  SlackParInfo(ASTString bname, Id* orig_id, Expression* r)
      : basename{bname}, id{orig_id}, range{r} {}
};

Slackify::Slackify() {}

std::string Slackify::get_name() { return "slackify"; }

struct IdReplacer : public MiniZinc::EVisitor {
  bool enter(MiniZinc::Expression* e) { return e; };

  void vId(Id* id) { id->v(id->decl()->id()->v()); }
};

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
      slack_pars.emplace_back(vd->id()->str(), vd->id(), ca->arg(0));
      ann.removeCall(slack_par);
    }
  }

  for (const SlackParInfo& spi : slack_pars) {
    std::stringstream ss;
    ss << spi.basename << "_slack";
    std::string name = ss.str();

    spi.id->v(ASTString(name));
    spi.id->decl()->id()->v(ASTString(name));
    spi.id->decl()->ti()->mkVar(e->envi());
  }

  for (const ConstraintI& ci : m->constraints()) {
    IdReplacer t;
    top_down(t, ci.e());
  }

  vector<Expression*> all_slacks;

  for (const SlackParInfo& spi : slack_pars) {
    // Add new vars
    VarDecl* vd = Expression::dynamicCast<VarDecl>(spi.id->decl());
    Id* newbaseid = new Id(Expression::loc(spi.id), spi.basename, nullptr);
    TypeInst* newTi = Expression::dynamicCast<TypeInst>(copy(e->envi(), vd->ti()));
    newTi->mkPar(e->envi());

    VarDecl* newBasePar = new VarDecl(Expression::loc(spi.id), newTi, newbaseid, nullptr);

    VarDeclI* basevdi = VarDeclI::a(Expression::loc(spi.id), newBasePar);
    m->addItem(basevdi);

    std::stringstream upss;
    upss << spi.basename << "_slack_up";
    std::string upname = upss.str();
    Id* newupid = new Id(Expression::loc(spi.id), upname, nullptr);

    TypeInst* upti = Expression::dynamicCast<TypeInst>(copy(e->envi(), vd->ti()));
    upti->domain(spi.range);

    VarDecl* newSlackUp = new VarDecl(Expression::loc(spi.id), upti, newupid, nullptr);

    VarDeclI* upvdi = VarDeclI::a(Expression::loc(spi.id), newSlackUp);
    m->addItem(upvdi);

    all_slacks.push_back(newupid);

    std::stringstream downss;
    downss << spi.basename << "_slack_down";
    std::string downname = downss.str();
    Id* newdownid = new Id(Expression::loc(spi.id), downname, nullptr);

    TypeInst* downti = Expression::dynamicCast<TypeInst>(copy(e->envi(), vd->ti()));
    downti->domain(spi.range);

    VarDecl* newSlackDown = new VarDecl(Expression::loc(spi.id), downti, newdownid, nullptr);

    VarDeclI* downvdi = VarDeclI::a(Expression::loc(spi.id), newSlackDown);
    m->addItem(downvdi);
    all_slacks.push_back(newdownid);

    vector<Expression*> args;
    args.push_back(newbaseid);
    args.push_back(newupid);
    args.push_back(newdownid);
    args.push_back(spi.range);
    Call* ca = Call::a(Expression::loc(spi.id), ASTString("slack_link"), args);
    spi.id->decl()->e(ca);
  }

  vector<Expression*> args;
  args.push_back(array_concat(all_slacks));
  Call* obj_call = Call::a(Location().introduce(), ASTString("slack_opt_mode"), args);
  SolveI* si = m->solveItem();
  si->st(SolveI::SolveType::ST_MIN);
  si->e(obj_call);

  if (OutputI* oi = m->outputItem()) {
    oi->remove();
  }

  return e;
}

};  // namespace MznAnalyse
