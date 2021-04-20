#pragma once

#include "tool_pass.hh"
#include <string>
#include <vector>

class ReadModel : public MiniZinc::Pass {
private:
  std::string in_path;

public:
  ReadModel(const std::string &ip);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
