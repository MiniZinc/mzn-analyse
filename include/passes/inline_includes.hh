#pragma once

#include "tool_pass.hh"

namespace MznAnalyse {
class InlineIncludes : public ToolPass {
private:
  bool local_only;

public:
  InlineIncludes(bool lo = true);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
  std::string get_name() { return "inline-includes"; }
};
};  // namespace MznAnalyse
