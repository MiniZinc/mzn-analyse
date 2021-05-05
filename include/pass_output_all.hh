#pragma once

#include "tool_pass.hh"

#include <string>
#include <vector>

class OutputAll : public MiniZinc::Pass {
public:
  OutputAll();

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
