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

namespace MznTool {

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

string getTermTypeString(vector<string>& gens, vector<string>& wheres, vector<string>& coefs,
                         Expression* var) {
  const string minor_sep = "|";
  Location loc = var->loc();

  if (var->isa<Id>()) {
    loc = var->dynamicCast<Id>()->decl()->loc();
  }

  stringstream var_ss;
  var_ss << *var;

  stringstream ss;
  ss << "\n    {\n";
  ss << "      \"variable\": \"" << escape(var_ss.str(), false) << "\",\n";
  ss << "      \"coefficients\": [" << utils::join(coefs, ", ", true) << "],\n";
  ss << "      \"generators\": [" << utils::join(gens, ", ", true) << "],\n";
  ss << "      \"conditions\": [" << utils::join(wheres, ", ", true) << "],\n";
  ss << "      \"location\": \"" << escape(loc.filename().c_str(), false) << minor_sep
     << loc.firstLine() << minor_sep << loc.firstColumn() << minor_sep << loc.lastLine()
     << minor_sep << loc.lastColumn() << "\"\n";
  ss << "    }";

  return ss.str();
}

struct StackFrame {
  size_t gen_idx;
  size_t coef_idx;
  size_t where_idx;
  Expression* e;

  StackFrame(size_t g, size_t c, size_t w, Expression* exp) : gen_idx{g}, coef_idx{c}, where_idx{w}, e{exp} {}
};

struct EscapedStringStack {
  vector<string> entries;
  void push(const string& s) {
    entries.push_back(escape(s, false));
  }
  void popTo(size_t idx) {
    while(entries.size() > idx) {
      entries.pop_back();
    }
  }
  size_t size() {
    return entries.size();
  }
};

string getTermsJSON(unordered_map<Id*, Expression*>& assigns, Expression* root) {
  vector<string> term_strings;

  // For now just support:
  // sum(gens where clauses) (coef1 * var1 + coef2 * var2)
  EscapedStringStack gens;
  EscapedStringStack coefs;
  EscapedStringStack wheres;

  vector<StackFrame> stack;
  stack.emplace_back(0, 0, 0, root);

  while (!stack.empty()) {
    StackFrame frame = stack.back();

    // std::cerr << "Frame["<< stack.size() <<"]: " << frame.gen_idx << ", " << frame.coef_idx << " :: " << *frame.e << std::endl;

    gens.popTo(frame.gen_idx);
    coefs.popTo(frame.coef_idx);
    wheres.popTo(frame.where_idx);
    stack.pop_back();

    if (Call* call = frame.e->dynamicCast<Call>()) {
      if (call->id() == "sum") {
        Expression* body = nullptr;
        if (call->arg(0)->isa<Comprehension>()) {
          Comprehension* co = call->arg(0)->cast<Comprehension>();
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

          ArrayAccess* aa = new ArrayAccess(arg0->loc(), arg0, {vd->id()});
          body = aa;
        }
        stack.emplace_back(gens.size(), coefs.size(), wheres.size(), body);
      } else {
        term_strings.push_back(getTermTypeString(gens.entries, wheres.entries, coefs.entries, call));
      }
    } else if (BinOp* bo = frame.e->dynamicCast<BinOp>()) {
      if (bo->op() == BOT_MULT) {
        if (bo->lhs()->type().isPar()) {
          stringstream ss;
          ss << *bo->lhs();
          coefs.push(ss.str());
          stack.emplace_back(gens.size(), coefs.size(), wheres.size(), bo->rhs());
        } else if (bo->rhs()->type().isPar()) {
          stringstream ss;
          ss << *bo->rhs();
          coefs.push(ss.str());
          stack.emplace_back(gens.size(), coefs.size(), wheres.size(), bo->lhs());
        } else {
          term_strings.push_back(getTermTypeString(gens.entries, wheres.entries, coefs.entries, bo));
        }
      } else if (bo->op() == BOT_PLUS && bo->lhs()->type().isvar()) {
        stack.emplace_back(gens.size(), coefs.size(), wheres.size(), bo->lhs());
        stack.emplace_back(gens.size(), coefs.size(), wheres.size(), bo->rhs());
      } else if (bo->op() == BOT_MINUS) {
        stack.emplace_back(gens.size(), coefs.size(), wheres.size(), bo->lhs());
        coefs.push("-1");
        stack.emplace_back(gens.size(), coefs.size(), wheres.size(), bo->rhs());
      } else {
        std::cerr << "UNHANDLED BinOp type" << std::endl;
        exit(EXIT_FAILURE);
      }
    } else if (Id* id = frame.e->dynamicCast<Id>()) {
      auto it = assigns.find(id->decl()->id());
      bool post = true;
      if (it != assigns.end()) {
        stack.emplace_back(gens.size(), coefs.size(), wheres.size(), it->second);
        post = false;
      }
      if (id->decl()->e()) {
        stack.emplace_back(gens.size(), coefs.size(), wheres.size(), id->decl()->e());
        post = false;
      }
      if (post) {
        // It is just a plain ID
        if (id->decl()->id()->type().isPar()) {
          coefs.push(id->str().c_str());
        } else {
          // std::cerr << "getTermTypeString(..., " << *id << ")" << std::endl;
          term_strings.push_back(getTermTypeString(gens.entries, wheres.entries, coefs.entries, id));
        }
      }
    } else {
      // std::cerr << "getTermTypeString(..., " << *id << ")" << std::endl;
      term_strings.push_back(getTermTypeString(gens.entries, wheres.entries, coefs.entries, frame.e));
    }
  }

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
  while (Id* id = e->dynamicCast<Id>()) {
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
  unordered_map<Id*, Expression*> &assigns;

  AssignCollector(unordered_map<Id*, Expression*> &as);
  bool enter(MiniZinc::Expression* e);
};

AssignCollector::AssignCollector(unordered_map<Id*, Expression*> &as) : assigns{as} {};

bool AssignCollector::enter(MiniZinc::Expression* e) {
  if (BinOp* bo = e->dynamicCast<BinOp>()) {
    if (bo->op() == BOT_EQ) {
      if (Id* lhe = bo->lhs()->dynamicCast<Id>()) {
        assigns[lhe->decl()->id()] = bo->rhs();
      }
      if (Id* rhe = bo->rhs()->dynamicCast<Id>()) {
        assigns[rhe->decl()->id()] = bo->lhs();
      }
      return false;
    }
    return bo->op() == BOT_AND;
  }

  if (Call *c = e->dynamicCast<Call>()) {
    return string(c->id().c_str()) == "forall";
  }

  return true;
}

MiniZinc::Env* GetTermTypes::run(MiniZinc::Env* e, std::ostream& log) {
  // Collect functional assignments for objective processing
  Model* m = e->model();
  unordered_map<Id*, Expression*> assigns;
  AssignCollector ac {assigns};

  // Collect top level assigns
  for (ConstraintI& ci : m->constraints()) {
    top_down(ac, ci.e());
  }

  // Add coef annotations to objective terms
  json_output = getObjectiveTermsJSON(m->solveItem(), assigns);

  return e;
}

}  // namespace MznTool
