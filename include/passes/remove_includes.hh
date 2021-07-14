#pragma once

#include "tool_pass.hh"
#include <string>
#include <vector>

namespace MznTool {

class RemoveIncludes : public ToolPass {
private:
  std::vector<std::string> includes;

public:
  RemoveIncludes(const std::vector<std::string> &is);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};

}; // namespace MznTool
