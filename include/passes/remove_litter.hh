#pragma once

#include "tool_pass.hh"

namespace MznAnalyse {
class RemoveLitter : public ToolPass {
public:
  bool aggressive;
  RemoveLitter(bool agg);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
  std::string get_name() { return "remove-litter"; }
};
};  // namespace MznAnalyse
