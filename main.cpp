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
    if(s.empty()) return;
    if(s[0] == '"') return;
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
      if(e && !(
            e->isa<VarDecl>() ||
            e->isa<TypeInst>())) {
        p.print(e);
      }

      return e != nullptr;
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
  if(argc < 2) {
    std::cerr << argv[0] << ": Invalid arguments." << std::endl;
    std::cerr << "Usage: " << argv[0] << " <mzn>" << std::endl;
    return EXIT_FAILURE;
  }
  vector<string> mzn_paths;

  for(int i=1;i<argc;i++) {
    mzn_paths.push_back(argv[i]);
  }

  vector<string> includes;
  string mzn_stdlib_dir = FileUtils::share_directory();
  includes.push_back(mzn_stdlib_dir + "/std/");

  ExpressionExtractor ee;
  for(const string& path: mzn_paths) {
    Env env;
    Model* m = MiniZinc::parse(env, {path}, {}, "", "", includes, false, false, false, std::cerr);

    if(!m) {
      std::cerr << argv[0] << ": Failed to parse file" << std::endl;
      return EXIT_FAILURE;
    }

    iterItems(ee, m);

    delete m;
  }


  return EXIT_SUCCESS;
}
