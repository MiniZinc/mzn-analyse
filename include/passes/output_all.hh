#pragma once

#include <string>
#include <vector>

#include "tool_pass.hh"

namespace MznTool {

class OutputAll : public ToolPass {
public:
  OutputAll();

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
};
};  // namespace MznTool
