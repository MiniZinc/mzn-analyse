#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>

#include <minizinc/model.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/solver.hh>
#include <minizinc/astiterator.hh>

using namespace MiniZinc;
using std::string;
using std::vector;

struct UniquePrinter {
  std::unordered_set<string> seen;

  void print(Expression* e) {
    std::stringstream ss;
    MiniZinc::Printer p(ss, 0, true);
    p.print(e);
    string s = ss.str();
    if(seen.find(s) == seen.end()) {
      seen.insert(s);
      std::cout << s << "\n";
    }
  }
};

struct ExpressionExtractor : public EVisitor {
    UniquePrinter p;

    bool enter(Expression* e) {
      p.print(e);
      return true;
    }
};

int main(int argc, char**argv) {
  if(argc != 2) {
    std::cerr << argv[0] << ": Invalid arguments." << std::endl;
    return EXIT_FAILURE;
  }
  string mzn_path = argv[1];

  vector<string> includes;
  string mzn_stdlib_dir = FileUtils::share_directory();
  includes.push_back(mzn_stdlib_dir + "/std/");

  Env env;
  Model* m = MiniZinc::parse(env, {mzn_path}, {}, "", "", includes, false, false, false, std::cerr);

  ExpressionExtractor ee;
  for (MiniZinc::VarDeclIterator it = m->begin_vardecls(); it != m->end_vardecls(); ++it) {
    VarDecl* vd = it->e();
    topDown(ee, vd->id());
    topDown(ee, vd->ti());
    topDown(ee, vd->e());
  }
  for (MiniZinc::ConstraintIterator it = m->begin_constraints(); it != m->end_constraints(); ++it) {
    Expression* e = it->e();
    topDown(ee, e);
  }

  return EXIT_SUCCESS;
}
