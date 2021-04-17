#pragma once

#include "tool_pass.hh"

class FilterItems : public MiniZinc::Pass {
private:
  std::vector<MiniZinc::Item::ItemId> item_types;

public:
  FilterItems(const std::vector<MiniZinc::Item::ItemId> &types, bool omit = false);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
