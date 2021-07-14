#pragma once

#include "tool_pass.hh"

#include <string>
#include <vector>

namespace MznTool {

class OutputAll : public ToolPass {
public:
  OutputAll();

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
}; // namespace MznTool
