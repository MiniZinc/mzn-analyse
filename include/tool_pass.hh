#pragma once

#include <memory>
#include <minizinc/flatten.hh>
#include <ostream>
#include <vector>

namespace MznAnalyse {

class ToolPass : public MiniZinc::Pass {
public:
  virtual std::string get_name() { return "MznAnalyse"; };
  virtual void write_json(std::ostream& os){};
};

MiniZinc::Env* multiPassFlatten(MiniZinc::Env& e,
                                const std::vector<std::unique_ptr<MiniZinc::Pass>>& passes,
                                std::vector<std::string>& json_store, std::ostream& _log);

}  // namespace MznAnalyse
