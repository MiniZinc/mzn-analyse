#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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

string escape(const string &orig, bool html) {
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

string join(const vector<string> &strs, const string &sep, bool quote = false) {
  std::stringstream ss;
  for (size_t i = 0; i < strs.size(); i++) {
    if (i)
      ss << sep;
    if (quote) {
      ss << "\"" << strs[i] << "\"";
    } else {
      ss << strs[i];
    }
  }
  return ss.str();
}

string getTermTypeString(vector<string> &gens, vector<string> &coefs,
                         Expression *var) {
  const string minor_sep = "|";
  Location loc = var->loc();

  stringstream ss;
  ss << "\n    {\n";
  ss << "      \"variable\": \"" << *var << "\",\n";
  ss << "      \"coefficients\": [" << join(coefs, ", ", true) << "],\n";
  ss << "      \"generators\": [" << join(gens, ", ", true) << "],\n";
  ss << "      \"location\": \"" << escape(loc.filename().c_str(), false)
     << minor_sep << loc.firstLine() << minor_sep << loc.firstColumn()
     << minor_sep << loc.lastLine() << minor_sep << loc.lastColumn() << "\"\n";
  ss << "    }";

  return ss.str();
}

struct StackFrame {
  size_t gen_idx;
  size_t coef_idx;
  Expression *e;

  StackFrame(size_t g, size_t c, Expression *exp)
      : gen_idx{g}, coef_idx{c}, e{exp} {}
};

string getTermsJSON(unordered_map<Id *, Expression *> &assigns,
                    Expression *root) {
  vector<string> term_strings;

  // For now just support:
  // sum(gens where clauses) (coef1 * var1 + coef2 * var2)
  vector<string> gens;
  vector<string> coefs;

  vector<StackFrame> stack;
  stack.emplace_back(0, 0, root);

  while (!stack.empty()) {
    StackFrame frame = stack.back();
    while (gens.size() > frame.gen_idx) {
      gens.pop_back();
    }
    while (coefs.size() > frame.coef_idx) {
      coefs.pop_back();
    }
    stack.pop_back();

    if (Call *call = frame.e->dynamicCast<Call>()) {
      if (call->id() == "sum") {
        Comprehension *co = call->arg(0)->cast<Comprehension>();
        // collect generators
        for (size_t i = 0; i < co->numberOfGenerators(); i++) {
          Expression *in = co->in(i);
          for (size_t j = 0; j < co->numberOfDecls(i); j++) {
            stringstream ss;
            VarDecl *idx = co->decl(i, j);
            ss << *idx->id() << " in " << *in;
            gens.push_back(ss.str());
          }
        }
        stack.emplace_back(gens.size(), coefs.size(), co->e());
      } else {
        term_strings.push_back(getTermTypeString(gens, coefs, call));
      }
    } else if (BinOp *bo = frame.e->dynamicCast<BinOp>()) {
      if (bo->op() == BOT_MULT) {
        if (bo->lhs()->type().isPar()) {
          stringstream ss;
          ss << *bo->lhs();
          coefs.push_back(ss.str());
          stack.emplace_back(gens.size(), coefs.size(), bo->rhs());
        } else {
          stringstream ss;
          ss << *bo->rhs();
          coefs.push_back(ss.str());
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
    } else if (Id *id = frame.e->dynamicCast<Id>()) {
      auto it = assigns.find(id->decl()->id());
      if (it != assigns.end()) {
        stack.emplace_back(gens.size(), coefs.size(), it->second);
      } else {
        // It is just an ID
        coefs.push_back(id->str().c_str());
        term_strings.push_back(getTermTypeString(gens, coefs, id));
      }
    } else {
      term_strings.push_back(getTermTypeString(gens, coefs, frame.e));
    }
  }

  // The printing bit
  stringstream ss;
  ss << "{\n  \"term_types\": [" << join(term_strings, ", ") << "]\n}\n";
  return ss.str();
}

string getObjectiveTermsJSON(SolveI *si,
                             unordered_map<Id *, Expression *> &assigns) {
  Expression *obj_e = si->e();
  if (!obj_e) {
    std::cerr << "No objective function" << std::endl;
    return "";
  }

  Expression *e = obj_e;
  while (Id *id = e->dynamicCast<Id>()) {
    e = id->decl()->e();
    if (!e) {
      auto it = assigns.find(id->decl()->id());
      if (it != assigns.end()) {
        e = it->second;
      }
      if (!e)
        return "";
    }
  }
  if (!e)
    return "";

  return getTermsJSON(assigns, e);
}

namespace MznData {
void objective(Model *m, string& model_output, string& termtype_output) {
  // Collect functional assignments for objective processing
  unordered_map<Id *, Expression *> assigns;

  // Add data annotations
  for (ConstraintI &ci : m->constraints()) {
    if (BinOp *bo = ci.e()->dynamicCast<BinOp>()) {
      if (bo->op() == BOT_EQ) {
        if (Id *lhe = bo->lhs()->dynamicCast<Id>()) {
          assigns[lhe->decl()->id()] = bo->rhs();
        }
        if (Id *rhe = bo->rhs()->dynamicCast<Id>()) {
          assigns[rhe->decl()->id()] = bo->lhs();
        }
      }
    }
  }

  // Add coef annotations to objective terms
  string terms_json = getObjectiveTermsJSON(m->solveItem(), assigns);

  // Remove solve item and output
  m->solveItem()->remove();
  m->outputItem()->remove();
  m->compact();

  // Write model without solve item to file
  {
    std::ofstream of(model_output);
    std::cerr << "Writing solveless model to: " << model_output
              << std::endl;
    Printer pp(of, 80, false);
    pp.print(m);
    of.close();
  }

  // Write term types to json file
  {
    std::cerr << "Writing term types to: " << termtype_output << std::endl;
    std::ofstream of(termtype_output);
    of << terms_json;
    of.close();
  }

}
}; // namespace MznData
