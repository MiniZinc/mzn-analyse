#include "passes/get_term_types.hh"

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

string escape(const string& orig, bool html) {
  string repchars = "\\&\"\'<>";
  vector<string> repstrs;
  if (html)
    repstrs = {"\\", "&amp;", "&quot;", "&apos;", "&lt;", "&gt;"};
  else
    repstrs = {"\\\\", "&", "\\\"", "'", "<", ">"};

  stringstream out;
  size_t last = 0;
  size_t found = orig.find_first_of(repchars);
  while (found != string::npos) {
    out << orig.substr(last, found - last);
    for (size_t i = 0; i < repchars.size(); i++) {
      if (orig[found] == repchars[i]) {
        out << repstrs[i];
        break;
      }
    }
    last = found + 1;
    found = orig.find_first_of(repchars, found + 1);
  }
  out << orig.substr(last);
  return out.str();
}

string getTermTypeString(string name, vector<string>& gens, vector<string>& wheres,
                         vector<string>& coefs, Expression* var) {
  const string minor_sep = "|";
  Location loc = Expression::loc(var);

  if (Expression::isa<Id>(var)) {
    loc = Expression::loc(Expression::dynamicCast<Id>(var)->decl());
  }

  stringstream var_ss;
  var_ss << *var;

  stringstream ss;
  ss << "\n    {\n";
  ss << "      \"name\": \"" << escape(name, false) << "\",\n";
  ss << "      \"variable\": \"" << escape(var_ss.str(), false) << "\",\n";
  ss << "      \"coefficients\": [" << utils::join(coefs, ", ", true) << "],\n";
  ss << "      \"generators\": [" << utils::join(gens, ", ", true) << "],\n";
  ss << "      \"conditions\": [" << utils::join(wheres, ", ", true) << "],\n";
  if (!loc.filename().empty()) {
    ss << "      \"location\": \"" << escape(loc.filename().c_str(), false) << minor_sep
       << loc.firstLine() << minor_sep << loc.firstColumn() << minor_sep << loc.lastLine()
       << minor_sep << loc.lastColumn() << "\"\n";
  } else {
    ss << "      \"location\": \"\"\n";
  }
  ss << "    }";

  return ss.str();
}

struct StackFrame {
  string name;
  size_t gen_idx;
  size_t coef_idx;
  size_t where_idx;
  Expression* e;

  StackFrame(string pname, size_t g, size_t c, size_t w, Expression* exp)
      : name{pname}, gen_idx{g}, coef_idx{c}, where_idx{w}, e{exp} {}
};

struct EscapedStringStack {
  vector<string> entries;
  void push(const string& s) { entries.push_back(escape(s, false)); }
  void popTo(size_t idx) {
    while (entries.size() > idx) {
      entries.pop_back();
    }
  }
  size_t size() { return entries.size(); }
};

Expression* negate(Expression* e) { return new UnOp(Location().introduce(), UOT_NOT, e); }

Expression* conj(vector<Expression*> exprs) {
  if (exprs.empty()) return nullptr;
  if (exprs.size() == 1) {
    return exprs[0];
  }
  BinOp* bo = new BinOp(Location().introduce(), exprs[0], BOT_AND, exprs[1]);
  for (size_t i = 2; i < exprs.size(); i++) {
    bo = new BinOp(Location().introduce(), bo, BOT_AND, exprs[i]);
  }
  return bo;
}

void collectTermStrings(unordered_map<Id*, Expression*>& assigns, vector<string>& term_strings,
                        string name, EscapedStringStack& gens, EscapedStringStack& coefs,
                        EscapedStringStack& wheres, Expression* root) {
  vector<StackFrame> stack;
  stack.emplace_back(name, gens.size(), coefs.size(), wheres.size(), root);

  while (!stack.empty()) {
    StackFrame frame = stack.back();

    // std::cerr << "Frame["<< stack.size() <<"]: " << frame.gen_idx << ", " << frame.coef_idx << ",
    // " << frame.where_idx << " :: " << *frame.e << std::endl;

    gens.popTo(frame.gen_idx);
    coefs.popTo(frame.coef_idx);
    wheres.popTo(frame.where_idx);
    stack.pop_back();

    if (Call* call = Expression::dynamicCast<Call>(frame.e)) {
      if (call->id() == "sum") {
        Expression* body = nullptr;
        if (Expression::isa<Comprehension>(call->arg(0))) {
          Comprehension* co = Expression::cast<Comprehension>(call->arg(0));
          // collect generators
          for (size_t i = 0; i < co->numberOfGenerators(); i++) {
            Expression* in = co->in(i);
            for (size_t j = 0; j < co->numberOfDecls(i); j++) {
              stringstream ss;
              VarDecl* idx = co->decl(i, j);
              ss << *idx->id() << " in " << *in;
              gens.push(ss.str());
            }
            Expression* where_e = co->where(i);
            if (where_e) {
              stringstream where_ss;
              where_ss << *where_e;
              wheres.push(where_ss.str());
            }
          }
          body = co->e();
        } else {
          // Otherwise:
          //   arg0: X
          //   gen: i in index_set(X)
          //   body: X[i]
          VarDecl* vd = new VarDecl(Location().introduce(),
                                    new TypeInst(Location().introduce(), Type::parint()), "i");
          Expression* arg0 = call->arg(0);

          stringstream ss;
          ss << "i in index_set(" << *arg0 << ")";
          gens.push(ss.str());

          ArrayAccess* aa = new ArrayAccess(Expression::loc(arg0), arg0, {vd->id()});
          body = aa;
        }
        stack.emplace_back(frame.name, gens.size(), coefs.size(), wheres.size(), body);
      } else {
        term_strings.push_back(
            getTermTypeString(frame.name, gens.entries, wheres.entries, coefs.entries, call));
      }
    } else if (BinOp* bo = Expression::dynamicCast<BinOp>(frame.e)) {
      if (bo->op() == BOT_MULT) {
        if (Expression::type(bo->lhs()).isPar()) {
          stringstream ss;
          ss << *bo->lhs();
          coefs.push(ss.str());
          stack.emplace_back(frame.name, gens.size(), coefs.size(), wheres.size(), bo->rhs());
        } else if (Expression::type(bo->rhs()).isPar()) {
          stringstream ss;
          ss << *bo->rhs();
          coefs.push(ss.str());
          stack.emplace_back(frame.name, gens.size(), coefs.size(), wheres.size(), bo->lhs());
        } else {
          term_strings.push_back(
              getTermTypeString(frame.name, gens.entries, wheres.entries, coefs.entries, bo));
        }
      } else if (bo->op() == BOT_PLUS && Expression::type(bo->lhs()).isvar()) {
        if (Expression::type(bo->lhs()).isPar()) {
          term_strings.push_back(getTermTypeString(frame.name, gens.entries, wheres.entries,
                                                   coefs.entries, bo->lhs()));
        } else {
          stack.emplace_back(frame.name, gens.size(), coefs.size(), wheres.size(), bo->lhs());
        }
        if (Expression::type(bo->rhs()).isPar()) {
          term_strings.push_back(getTermTypeString(frame.name, gens.entries, wheres.entries,
                                                   coefs.entries, bo->rhs()));
        } else {
          stack.emplace_back(frame.name, gens.size(), coefs.size(), wheres.size(), bo->rhs());
        }
      } else if (bo->op() == BOT_MINUS) {
        if (Expression::type(bo->lhs()).isPar()) {
          term_strings.push_back(getTermTypeString(frame.name, gens.entries, wheres.entries,
                                                   coefs.entries, bo->lhs()));
        } else {
          stack.emplace_back(frame.name, gens.size(), coefs.size(), wheres.size(), bo->lhs());
        }
        coefs.push("-1");
        if (Expression::type(bo->rhs()).isPar()) {
          term_strings.push_back(getTermTypeString(frame.name, gens.entries, wheres.entries,
                                                   coefs.entries, bo->rhs()));
        } else {
          stack.emplace_back(frame.name, gens.size(), coefs.size(), wheres.size(), bo->rhs());
        }
      } else {
        std::cerr << "UNHANDLED BinOp type" << std::endl;
        exit(EXIT_FAILURE);
      }
    } else if (Id* id = Expression::dynamicCast<Id>(frame.e)) {
      auto it = assigns.find(id->decl()->id());

      bool post = true;
      if (it != assigns.end()) {
        stack.emplace_back(id->decl()->id()->str().c_str(), gens.size(), coefs.size(),
                           wheres.size(), it->second);
        post = false;
      }
      if (id->decl()->e()) {
        stack.emplace_back(id->decl()->id()->str().c_str(), gens.size(), coefs.size(),
                           wheres.size(), id->decl()->e());
        post = false;
      }
      if (post) {
        // It is just a plain ID. It doesn't matter if it is par or not
        term_strings.push_back(
            getTermTypeString(frame.name, gens.entries, wheres.entries, coefs.entries, id));
      }
    } else if (ITE* ite = Expression::dynamicCast<ITE>(frame.e)) {
      vector<Expression*> negs;
      for (size_t i = 0; i < ite->size(); i++) {
        Expression* currentIf = ite->ifExpr(i);

        vector<Expression*> negs_if(negs);
        negs_if.push_back(currentIf);

        stringstream where_ss;
        where_ss << *conj(negs_if);
        wheres.push(where_ss.str());

        collectTermStrings(assigns, term_strings, frame.name, gens, coefs, wheres,
                           ite->thenExpr(i));
        wheres.popTo(frame.where_idx);

        negs.push_back(negate(currentIf));
      }

      stringstream where_ss;
      where_ss << *conj(negs);
      wheres.push(where_ss.str());
      collectTermStrings(assigns, term_strings, frame.name, gens, coefs, wheres, ite->elseExpr());
      wheres.popTo(frame.where_idx);

    } else {
      // std::cerr << "getTermTypeString(..., " << *frame.e << ")" << std::endl;
      term_strings.push_back(
          getTermTypeString(frame.name, gens.entries, wheres.entries, coefs.entries, frame.e));
    }
  }
}

string getTermsJSON(unordered_map<Id*, Expression*>& assigns, Expression* root) {
  vector<string> term_strings;

  // For now just support:
  // sum(gens where clauses) (coef1 * var1 + coef2 * var2)
  EscapedStringStack gens;
  EscapedStringStack coefs;
  EscapedStringStack wheres;

  collectTermStrings(assigns, term_strings, "", gens, coefs, wheres, root);

  // The printing bit
  stringstream ss;
  ss << "[" << utils::join(term_strings, ", ") << "]";
  return ss.str();
}

string getObjectiveTermsJSON(SolveI* si, unordered_map<Id*, Expression*>& assigns) {
  if (!si || si->st() == SolveI::ST_SAT) {
    return "";
  }

  Expression* obj_e = si->e();
  if (!obj_e) {
    std::cerr << "No objective function" << std::endl;
    return "";
  }

  Expression* e = obj_e;
  while (Id* id = Expression::dynamicCast<Id>(e)) {
    e = id->decl()->e();
    if (!e) {
      auto it = assigns.find(id->decl()->id());
      if (it != assigns.end()) {
        e = it->second;
      }
      if (!e) return "";
    }
  }
  if (!e) return "";

  return getTermsJSON(assigns, e);
}

GetTermTypes::GetTermTypes() {}

std::string GetTermTypes::get_name() { return "get-term-types"; }

void GetTermTypes::write_json(std::ostream& os) { os << json_output; }

struct AssignCollector : public MiniZinc::EVisitor {
  unordered_map<Id*, Expression*>& assigns;

  AssignCollector(unordered_map<Id*, Expression*>& as);
  bool enter(MiniZinc::Expression* e);
};

AssignCollector::AssignCollector(unordered_map<Id*, Expression*>& as) : assigns{as} {};

bool AssignCollector::enter(MiniZinc::Expression* e) {
  if (BinOp* bo = Expression::dynamicCast<BinOp>(e)) {
    if (bo->op() == BOT_EQ) {
      if (Id* lhe = Expression::dynamicCast<Id>(bo->lhs())) {
        assigns[lhe->decl()->id()] = bo->rhs();
      } else if (Id* rhe = Expression::dynamicCast<Id>(bo->rhs())) {
        assigns[rhe->decl()->id()] = bo->lhs();
      }
      return false;
    }
    return bo->op() == BOT_AND;
  }

  if (Call* c = Expression::dynamicCast<Call>(e)) {
    return string(c->id().c_str()) == "forall";
  }

  return true;
}

MiniZinc::Env* GetTermTypes::run(MiniZinc::Env* e, std::ostream& log) {
  // Collect functional assignments for objective processing
  Model* m = e->model();
  unordered_map<Id*, Expression*> assigns;
  AssignCollector ac{assigns};

  // Collect top level assigns
  for (ConstraintI& ci : m->constraints()) {
    top_down(ac, ci.e());
  }

  // Add coef annotations to objective terms
  json_output = getObjectiveTermsJSON(m->solveItem(), assigns);

  return e;
}

}  // namespace MznAnalyse
