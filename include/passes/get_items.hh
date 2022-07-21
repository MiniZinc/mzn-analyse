#pragma once

#include <set>

#include "tool_pass.hh"

namespace MznTool {

class GetItems : public ToolPass {
private:
  std::set<size_t> indexes;

public:
  GetItems(const std::vector<size_t>& idxs);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
};

};  // namespace MznTool
