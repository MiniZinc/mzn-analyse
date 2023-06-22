#pragma once

#include "tool_pass.hh"

namespace MznAnalyse {
class RemoveLitter : public ToolPass {
public:
  RemoveLitter();

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
};
};  // namespace MznAnalyse
