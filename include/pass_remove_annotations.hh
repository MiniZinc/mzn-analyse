#pragma once

#include "tool_pass.hh"

#include <string>
#include <vector>

class RemoveAnnotations : public MiniZinc::Pass {
private:
  std::vector<std::string> ann_names;

public:
  RemoveAnnotations(std::string ann_name);
  RemoveAnnotations(std::vector<std::string> &ann_names);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
