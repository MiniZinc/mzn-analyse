#include "pass_get_data_deps.hh"

#include <algorithm>
#include <fstream>
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

int prec(string s) {
  if (s == "in")
    return 0;
  if (s == "if_eq")
    return 1;
  if (s == "if")
    return 2;
  if (s == "if_exists")
    return 3;
  if (s == "lit")
    return 4;
  return 10;
}

ostream &operator<<(ostream &os, vector<Call *> &calls) {
  std::sort(calls.begin(), calls.end(), [](const auto &lhs, const auto &rhs) {
    int l_depth = lhs->arg(0)->template cast<IntLit>()->v().toInt();
    int r_depth = rhs->arg(0)->template cast<IntLit>()->v().toInt();

    int l_type = prec(lhs->arg(1)->template cast<StringLit>()->v().c_str());
    int r_type = prec(rhs->arg(1)->template cast<StringLit>()->v().c_str());

    return l_depth < r_depth || (l_depth == r_depth && l_type < r_type);
  });

  if (calls.empty()) {
    os << "[]";
  } else {
    os << "[\n";
    for (int i = 0; i < calls.size(); i++) {
      Call *ca = calls[i];
      string type = ca->arg(1)->cast<StringLit>()->v().c_str();

      os << "    ["
         << "\"" << type << "\", ";

      if (type == "lit") {
        os << *ca->arg(2);
      } else if (type == "if") {
        os << *ca->arg(2);
      } else if (type == "eq" || type == "assign") {
        os << *ca->arg(2) << ", " << *ca->arg(3);
      } else if (type == "in") {
        os << *ca->arg(2) << ", " << *ca->arg(3);
      } else {
        std::cerr << "UNKNOWN ANNOTATION: " << *ca << std::endl;
      }

      os << "]" << (i != calls.size() - 1 ? "," : "")
         << (i != calls.size() - 1 ? "\n" : "");
    }
    os << "]";
  }
  return os;
}

GetDataDeps::GetDataDeps(const std::string& out_path) : output_path{out_path} {}

MiniZinc::Env* GetDataDeps::run(MiniZinc::Env* e, std::ostream& log) {
  Model* m = e->model();
  // Collect and write data entries
  string fzn_path = m->filepath().c_str();

  std::cerr << "Writing constraint data to: " << output_path << std::endl;
  std::ofstream out_json_os{output_path};
  out_json_os << "{\"constraint_info\": [\n";

  bool first = true;
  for (ConstraintI &ci : m->constraints()) {
    vector<Call *> entries;

    if (first) {
      first = false;
    } else {
      out_json_os << ",\n";
    }

    for (Call *ca = ci.e()->ann().getCall(ASTString("data")); ca != nullptr;
         ca = ci.e()->ann().getCall(ASTString("data"))) {
      entries.push_back(ca);
      ci.e()->ann().remove(ca);
    }

    out_json_os << "  " << entries;
  }

  out_json_os << "]}" << std::endl;
  out_json_os.close();

  return e;
}
