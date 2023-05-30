#include "passes/get_data_deps.hh"

#include <fstream>
#include <iostream>
#include <minizinc/astiterator.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>
#include <sstream>
#include <string>
#include <vector>

#include "string_utils.hh"

using namespace MiniZinc;
using std::ostream;
using std::string;
using std::vector;

namespace MznTool {

int prec(string s) {
  if (s == "in") return 0;
  if (s == "if_eq") return 1;
  if (s == "if") return 2;
  if (s == "if_exists") return 3;
  if (s == "lit") return 4;
  if (s == "assign") return 5;
  if (s == "eq") return 6;
  return 10;
}

ostream& operator<<(ostream& os, vector<Call*>& calls) {
  std::sort(calls.begin(), calls.end(), [](const auto& lhs, const auto& rhs) {
    int l_depth =     IntLit::v(Expression::template cast<IntLit>(lhs->arg(0))).toInt();
    int r_depth =     IntLit::v(Expression::template cast<IntLit>(rhs->arg(0))).toInt();

    int l_index =     IntLit::v(Expression::template cast<IntLit>(lhs->arg(1))).toInt();
    int r_index =     IntLit::v(Expression::template cast<IntLit>(rhs->arg(1))).toInt();

    int l_type = prec(Expression::template cast<StringLit>(lhs->arg(2))->v().c_str());
    int r_type = prec(Expression::template cast<StringLit>(rhs->arg(2))->v().c_str());

    return l_depth < r_depth ||
           (l_depth == r_depth && (l_type < r_type || (l_type == r_type && l_index > r_index)));
  });

  if (calls.empty()) {
    os << "[]";
  } else {
    os << "[\n";
    for (int i = 0; i < calls.size(); i++) {
      Call* ca = calls[i];
      string type = Expression::cast<StringLit>(ca->arg(2))->v().c_str();

      os << "    ["
         << "\"" << type << "\", ";

      if (type == "lit") {
        os << *ca->arg(3);
      } else if (type == "if") {
        os << *ca->arg(3);
      } else if (type == "eq" || type == "assign") {
        os << *ca->arg(3) << ", " << *ca->arg(4);
      } else if (type == "in") {
        os << *ca->arg(3) << ", " << *ca->arg(4);
      } else {
        std::cerr << "UNKNOWN ANNOTATION: " << *ca << std::endl;
      }

      os << "]" << (i != calls.size() - 1 ? "," : "") << (i != calls.size() - 1 ? "\n" : "");
    }
    os << "]";
  }
  return os;
}

void GetDataDeps::write_json(ostream& os) {
  std::vector<std::string> constraint_info;
  for (auto& it : data_deps) {
    std::stringstream info_ss;
    info_ss << "  \"" << it.first << "\": " << it.second;
    constraint_info.push_back(info_ss.str());
  }

  os << "{\n" << utils::join(constraint_info, ",\n") << "}\n";
}

GetDataDeps::GetDataDeps() {}

std::string GetDataDeps::get_name() { return "get-data-deps"; }

MiniZinc::Env* GetDataDeps::run(MiniZinc::Env* e, std::ostream& log) {
  Model* m = e->model();

  collect_data_deps(m);

  return e;
}

void GetDataDeps::collect_data_deps(Model* m) {
  size_t c_id = 0;
  for (ConstraintI& ci : m->constraints()) {
    vector<Call*> entries;

    for (Expression* ann_e : Expression::ann(ci.e())) {
      Call* ca = Expression::dynamicCast<Call>(ann_e);
      if (ca && ca->id() == "data") {
        entries.push_back(ca);
      }
    }

    if (!entries.empty()) {
      data_deps[c_id] = entries;
    }
    c_id++;
  }
}
};  // namespace MznTool
