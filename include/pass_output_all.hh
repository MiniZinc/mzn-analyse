#pragma once

#include "tool_pass.hh"

#include <string>
#include <vector>

class OutputAll : public ToolPass {
public:
  OutputAll();

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
