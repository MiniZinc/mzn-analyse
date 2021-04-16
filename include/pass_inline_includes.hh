#pragma once

#include "tool_pass.hh"

class InlineIncludes : public MiniZinc::Pass {
private:
  bool local_only;

public:
  InlineIncludes(bool lo = true);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
