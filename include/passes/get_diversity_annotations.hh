#pragma once

#include "tool_pass.hh"

#include <string>

namespace MznTool {

struct VarInfo {
  std::string name;
  std::string prev_name;
  std::string type;
  std::string prev_type;
  std::string distance_function;
};

struct ObjInfo {
  std::string name;
  double sense;
};

struct DiversityOptions {
  int k;             // Number of solutions to find
  double gap;        // Acceptable distance from optimal
  std::string type;  // iterative/global
  ObjInfo objective; // single variable name
  std::vector<VarInfo> vars;
  std::string inter_diversity_constraint;
  std::string intra_diversity_constraint;
  std::string aggregator;
  std::string combinator;
};

class GetDiversityAnns : public ToolPass {
private:
  DiversityOptions div_opts;

  void collect_diversity_annotations(MiniZinc::Env *e, MiniZinc::Model *m);

public:
  GetDiversityAnns();

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;

  void write_json(std::ostream &os) override;
  std::string get_name() override;
};

}; // namespace MznTool
