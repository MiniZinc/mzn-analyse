#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "tool_pass.hh"

namespace MznAnalyse {
class JSONTool : public ToolPass {
public:
  enum JSONCommand { J_Output, J_Clear };

private:
  std::vector<std::string>& json_store;
  JSONCommand command;
  std::string out_path;

public:
  JSONTool(std::vector<std::string>& store, JSONCommand cmd, const std::string& out);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
};
};  // namespace MznAnalyse
