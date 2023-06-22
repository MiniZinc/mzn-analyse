#include "passes/get_solve_anns.hh"

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

#include "passes/get_term_types.hh"
#include "string_utils.hh"

using namespace MiniZinc;
using std::string;
using std::stringstream;
using std::unordered_map;
using std::vector;

namespace MznAnalyse {

GetSolveAnns::GetSolveAnns() {}

std::string GetSolveAnns::get_name() { return "get-solve-anns"; }

void GetSolveAnns::write_json(std::ostream& os) { os << json_output; }

std::string getSolveAnns(const SolveI* si) {
  vector<string> anns;

  for (auto& ann : si->ann()) {
    std::stringstream ss;
    ss << *ann;
    anns.push_back("\"" + utils::escape(ss.str(), false) + "\"");
  }

  std::stringstream ssa;
  ssa << "[";
  ssa << utils::join(anns, ",");
  ssa << "]";

  return ssa.str();
}

MiniZinc::Env* GetSolveAnns::run(MiniZinc::Env* e, std::ostream& log) {
  // Collect functional assignments for objective processing
  Model* m = e->model();

  // Prettyprint solve anns
  json_output = getSolveAnns(m->solveItem());

  return e;
}

}  // namespace MznAnalyse
