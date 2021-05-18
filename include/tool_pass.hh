#pragma once

#include <memory>
#include <minizinc/flatten.hh>
#include <ostream>
#include <vector>

class ToolPass : public MiniZinc::Pass {
  virtual void write_json(std::ostream &os){};
};

MiniZinc::Env *
multiPassFlatten(MiniZinc::Env &e,
                 const std::vector<std::unique_ptr<MiniZinc::Pass>> &passes,
                 std::ostream &_log);
