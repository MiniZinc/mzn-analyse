#pragma once

#include <string>
#include <vector>

#include "location_utils.hh"
#include "tool_pass.hh"

namespace MznAnalyse {
class LetSubstituter : public ToolPass {
private:
  std::vector<ShortLoc> locs;
  std::string json_output;

public:
  LetSubstituter(const std::vector<std::string>& paths);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
  std::string get_name() { return "let-substituteer"; }
  void write_json(std::ostream& os) override;
};
};  // namespace MznAnalyse
