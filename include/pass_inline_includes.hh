#pragma once

#include "tool_pass.hh"

class InlineIncludes : public ToolPass {

public:
  InlineIncludes();

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
