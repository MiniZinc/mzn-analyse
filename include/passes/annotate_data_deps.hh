#pragma once

#include "tool_pass.hh"

namespace MznTool {

class AnnotateDataDeps : public ToolPass {
public:
  AnnotateDataDeps();

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
};

};  // namespace MznTool
