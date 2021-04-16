#pragma once

#include "tool_pass.hh"

#include <string>

class GetDataDeps : public MiniZinc::Pass {
private:
  std::string output_path;

public:
  GetDataDeps(const std::string &out_path);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
