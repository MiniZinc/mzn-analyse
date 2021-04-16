#pragma once

#include "tool_pass.hh"

class AnnotateDataDeps : public ToolPass {

public:
  AnnotateDataDeps();

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
