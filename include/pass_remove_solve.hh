#pragma once

#include "tool_pass.hh"

class RemoveSolve : public ToolPass {

public:
  RemoveSolve();

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
