#pragma once

#include "tool_pass.hh"

class RemoveStdlib : public ToolPass {

public:
  RemoveStdlib();

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
};

