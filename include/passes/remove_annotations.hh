#pragma once

#include <string>
#include <vector>

#include "tool_pass.hh"
namespace MznAnalyse {
class RemoveAnnotations : public ToolPass {
private:
  std::vector<std::string> ann_names;

public:
  RemoveAnnotations(const std::vector<std::string>& ann_names);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
};
};  // namespace MznAnalyse
