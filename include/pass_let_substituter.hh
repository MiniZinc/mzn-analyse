#pragma once

#include "tool_pass.hh"
#include "location_utils.hh"

#include <string>
#include <vector>

class LetSubstituter : public MiniZinc::Pass {
private:
  std::vector<ShortLoc> locs;

public:
  LetSubstituter(const std::vector<std::string>& paths);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
