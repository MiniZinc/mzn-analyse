#pragma once

#include "tool_pass.hh"

#include <string>
#include <vector>

class GetExprs : public MiniZinc::Pass {
private:
  std::vector<std::string> mzn_paths;
  std::string output_path;

public:
  GetExprs(const std::string &out_path);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
