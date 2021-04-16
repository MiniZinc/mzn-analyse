#pragma once

#include "tool_pass.hh"

#include <string>

class GetTermTypes : public ToolPass {
private:
  std::string output_path;

public:
  GetTermTypes(const std::string& out_path);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
};

