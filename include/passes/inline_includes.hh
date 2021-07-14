#pragma once

#include "tool_pass.hh"

namespace MznTool {
class InlineIncludes : public ToolPass {
private:
  bool local_only;

public:
  InlineIncludes(bool lo = true);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
}; // namespace MznTool
