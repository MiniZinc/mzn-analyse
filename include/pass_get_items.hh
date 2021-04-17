#pragma once

#include "tool_pass.hh"
#include <set>

class GetItems : public MiniZinc::Pass {
private:
  std::set<size_t> indexes;

public:
  GetItems(const std::vector<size_t> &idxs);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
