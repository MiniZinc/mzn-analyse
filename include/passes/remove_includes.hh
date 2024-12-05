#pragma once

#include <string>
#include <vector>

#include "tool_pass.hh"

namespace MznAnalyse {

class RemoveIncludes : public ToolPass {
private:
  std::vector<std::string> includes;

public:
  RemoveIncludes(const std::vector<std::string>& is);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
  std::string get_name() { return "remove-includes"; }
};

};  // namespace MznAnalyse
