#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>

#include <minizinc/model.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/solver.hh>
#include <minizinc/astiterator.hh>
#include <minizinc/prettyprinter.hh>

using namespace MiniZinc;
using std::string;
using std::vector;
using std::ostream;

int prec(string s) {
  if (s == "in") return 0;
  if (s == "if_eq") return 1;
  if (s == "if") return 2;
  if (s == "if_exists") return 3;
  if (s == "lit") return 4;
  return 10;
}

ostream& operator<<(ostream& os, vector<Call*>& calls) {
  std::sort(calls.begin(), calls.end(), [](const auto& lhs, const auto& rhs) {
    int l_depth = lhs->arg(0)->template cast<IntLit>()->v().toInt();
    int r_depth = rhs->arg(0)->template cast<IntLit>()->v().toInt();

    int l_type = prec(lhs->arg(1)->template cast<StringLit>()->v().c_str());
    int r_type = prec(rhs->arg(1)->template cast<StringLit>()->v().c_str());

    return l_depth < r_depth ||
           (l_depth == r_depth && l_type < r_type);
  });

  if(calls.empty()) {
    os << "[]";
  } else {
    os << "[\n";
    for(int i=0; i<calls.size(); i++) {
      Call* ca = calls[i];
      string type = ca->arg(1)->cast<StringLit>()->v().c_str();

      os << "    ["
        << "\"" << type << "\", ";

      if(type == "lit") {
        os << *ca->arg(2);
      } else if(type == "if") {
        os << *ca->arg(2);
      } else if(type == "eq" || type == "assign") {
        os << *ca->arg(2) << ", " << *ca->arg(3);
      } else if(type == "in") {
        os << *ca->arg(2) << ", " << *ca->arg(3);
      } else {
        std::cerr << "UNKNOWN ANNOTATION: " << *ca << std::endl;
      }

      os << "]" << (i != calls.size()-1 ? "," : "") << (i != calls.size()-1 ? "\n" : "");
    }
    os << "]";
  }
  return os;
}

namespace MznData {
  void extract(Model* m) {
    // Collect and write data entries
    string fzn_path = m->filepath().c_str();
    string output_base = fzn_path.substr(0, fzn_path.size() - 4);
    string out_json = output_base + ".cons";
    std::cerr << "Writing constraint data to: " << out_json << std::endl;
    std::ofstream out_json_os {out_json};
    out_json_os << "{\"constraint_info\": [\n";

    bool first = true;
    for(ConstraintI& ci : m->constraints()) {
      vector<Call*> entries;

      if(first) {
        first = false;
      } else {
        out_json_os << ",\n";
      }

      for(Call* ca = ci.e()->ann().getCall(ASTString("data"));
          ca != nullptr;
          ca = ci.e()->ann().getCall(ASTString("data"))) {
        entries.push_back(ca);
        ci.e()->ann().remove(ca);
      }

      out_json_os << "  " << entries;
    }

    out_json_os << "]}" << std::endl;
    out_json_os.close();

    // Write clean fzn file
    std::cerr << "Overwriting original fzn." << std::endl;
    std::ofstream out_fzn_os {fzn_path};
    Printer p(out_fzn_os, 0, true);
    p.print(m);
    out_fzn_os.close();
  }
};
