#pragma once

#include "tool_pass.hh"

namespace MznAnalyse {

class AnnotateDataDeps : public ToolPass {
public:
  AnnotateDataDeps();

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;

  std::string get_name() { return "discretise"; }
};

};  // namespace MznAnalyse
