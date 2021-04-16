#pragma once

#include "tool_pass.hh"

class RemoveOutput : public ToolPass {

public:
  RemoveOutput();

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
