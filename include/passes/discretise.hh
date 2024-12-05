#pragma once

#include <string>

#include "tool_pass.hh"

namespace MznAnalyse {

class Discretise : public ToolPass {
private:
  unsigned int base_scale_factor;

public:
  Discretise(unsigned int base_scale);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
  std::string get_name() { return "discretise"; }
};
};  // namespace MznAnalyse
