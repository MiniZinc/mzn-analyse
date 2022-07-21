#pragma once

#include <string>
#include <vector>

#include "tool_pass.hh"

namespace MznTool {

class ReadModel : public ToolPass {
private:
  std::string in_path;

public:
  ReadModel(const std::string& ip);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
};

};  // namespace MznTool
