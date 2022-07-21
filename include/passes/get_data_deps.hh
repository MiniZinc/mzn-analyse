#pragma once

#include <string>

#include "tool_pass.hh"

namespace MznTool {

class GetDataDeps : public ToolPass {
private:
  std::unordered_map<size_t, std::vector<MiniZinc::Call*>> data_deps;
  void collect_data_deps(MiniZinc::Model* m);

public:
  GetDataDeps();

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;

  void write_json(std::ostream& os) override;
  std::string get_name() override;
};

};  // namespace MznTool
