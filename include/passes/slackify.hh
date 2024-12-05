#pragma once

#include <string>

#include "tool_pass.hh"

namespace MznAnalyse {

class Slackify : public ToolPass {
public:
  Slackify();

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;

  std::string get_name() { return "slackify"; }

};

};  // namespace MznAnalyse
