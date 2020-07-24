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

  void print(std::string& s) {
    if(seen.find(s) == seen.end()) {
      seen.insert(s);
      std::cout << s << "\n";
    }
  }
  void print(Expression* e) {
    std::stringstream ss;
    MiniZinc::Printer p(ss, 0, true);
    p.print(e);
    std::string s = ss.str();
    print(s);
  }
};

struct ExpressionExtractorEVisitor : public EVisitor {
    UniquePrinter& p;

    ExpressionExtractorEVisitor(UniquePrinter& p1) : p{p1} {}

    bool enter(Expression* e) {
      p.print(e);
      return true;
    }
};

struct ExpressionExtractor : public ItemVisitor {
  UniquePrinter p;
  ExpressionExtractorEVisitor eev;
  ExpressionExtractor() : eev{p} {}


  bool enter(Item* item) {
    return !item->isa<IncludeI>();
  }
  void vVarDeclI(VarDeclI* vdi) {
    VarDecl* vd = vdi->e();
    topDown(eev, vd);
  }
  void vAssignI(AssignI* ai) {
    p.print(ai->id().str());
    topDown(eev, ai->decl());
    topDown(eev, ai->e());
  }
  void vConstraintI(ConstraintI* ci) {
    topDown(eev, ci->e());
  }
  void vSolveI(SolveI* si) {
    topDown(eev, si->e());
  }
  void vFunctionI(FunctionI* fi) {
    p.print(fi->id().str());
    topDown(eev, fi->ti());
    topDown(eev, fi->e());

    auto& params = fi->params();
    for(unsigned int i=0; i<params.size(); i++) {
      topDown(eev, params[i]);
    }

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

  iterItems(ee, m);

  return EXIT_SUCCESS;
}
