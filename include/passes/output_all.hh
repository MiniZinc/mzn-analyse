#pragma once

#include <string>
#include <vector>

#include "tool_pass.hh"

namespace MznAnalyse {

class OutputAll : public ToolPass {
public:
  OutputAll();

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
  std::string get_name() { return "output-all"; }
};
};  // namespace MznAnalyse
