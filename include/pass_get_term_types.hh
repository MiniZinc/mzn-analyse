#pragma once

#include "tool_pass.hh"

#include <string>

class GetTermTypes : public ToolPass {
private:
  std::string json_output;
  std::string output_path;

public:
  GetTermTypes(const std::string &out_path);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
  void write_json(std::ostream &os) override;
  std::string get_name() override;
};
