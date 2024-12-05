#pragma once

#include <string>
#include <vector>

#include "tool_pass.hh"

namespace MznAnalyse {

class ReadModel : public ToolPass {
private:
  bool is_fzn;
  std::vector<std::string> mzn_paths;
  std::vector<std::string> dzn_paths;

public:
  ReadModel(const std::string& ip);
  ReadModel(const std::vector<std::string>& ips);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;

  std::string get_name() { return "read-model"; }
};

};  // namespace MznAnalyse
