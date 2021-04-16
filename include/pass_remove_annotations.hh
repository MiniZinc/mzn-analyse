#pragma once

#include "tool_pass.hh"

#include <vector>
#include <string>

class RemoveAnnotations : public ToolPass {
  private:
    std::vector<std::string> ann_names;

public:
  RemoveAnnotations(std::string ann_name);
  RemoveAnnotations(std::vector<std::string>& ann_names);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;
};

