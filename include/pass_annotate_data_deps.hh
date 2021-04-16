#pragma once

#include "tool_pass.hh"

class AnnotateDataDeps : public MiniZinc::Pass {

public:
  AnnotateDataDeps();

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
