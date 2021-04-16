#pragma once

#include "tool_pass.hh"

class RemoveItems : public MiniZinc::Pass {
private:
  std::vector<MiniZinc::Item::ItemId> item_types;

public:
  RemoveItems(const std::vector<MiniZinc::Item::ItemId> &types);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
