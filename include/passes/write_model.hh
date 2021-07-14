#pragma once

#include "tool_pass.hh"
#include <string>
#include <vector>

namespace MznTool {

class WriteModel : public ToolPass {
private:
  std::string out_path;
  bool is_fzn;

public:
  WriteModel(const std::string &op, bool is_fzn = false);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
}; // namespace MznTool
