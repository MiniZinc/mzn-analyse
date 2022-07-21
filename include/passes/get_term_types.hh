#pragma once

#include <string>

#include "tool_pass.hh"

namespace MznTool {

class GetTermTypes : public ToolPass {
private:
  std::string json_output;

public:
  GetTermTypes();

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
  void write_json(std::ostream& os) override;
  std::string get_name() override;
};
};  // namespace MznTool
