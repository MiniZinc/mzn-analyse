#pragma once

#include <minizinc/ast.hh>
#include <string>
#include <vector>

#include "tool_pass.hh"

namespace MznTool {

struct VdInfo {
  std::string name;
  MiniZinc::Id* id;
  std::vector<std::string> renames;

  VdInfo(std::string vd_name, MiniZinc::Id* id_ptr) : name{vd_name}, id{id_ptr} {}
};

class RepeatModel : public ToolPass {
private:
  unsigned int k_models; 
  std::vector<VdInfo> vds;

public:
  RepeatModel(unsigned int k);

  void write_json(std::ostream& os) override;
  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
};

};  // namespace MznTool
