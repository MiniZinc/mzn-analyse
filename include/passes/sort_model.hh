#pragma once

#include "tool_pass.hh"

namespace MznAnalyse {

class SortModel : public ToolPass {
public:
  SortModel();

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
};
};  // namespace MznAnalyse
