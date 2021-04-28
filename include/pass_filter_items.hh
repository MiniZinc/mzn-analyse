#pragma once

#include "tool_pass.hh"

class FilterItems : public MiniZinc::Pass {
public:
  enum FilterTypeInst { ALL, VAR, PAR };

private:
  std::vector<MiniZinc::Item::ItemId> item_types;
  bool exclude;
  FilterTypeInst typeinst;

public:
  FilterItems(const std::vector<MiniZinc::Item::ItemId> &types,
              bool omit = false, FilterTypeInst ti = ALL);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;

  bool should_remove(MiniZinc::Item *item);
};
