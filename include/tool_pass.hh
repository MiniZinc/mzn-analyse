#pragma once

#include <memory>
#include <minizinc/flatten.hh>
#include <ostream>
#include <vector>

class ToolPass {
public:
  ToolPass(){};
  virtual MiniZinc::Env *run(MiniZinc::Env *env, std::ostream &log) = 0;
  virtual ~ToolPass(){};
};

MiniZinc::Env *
multiPassFlatten(MiniZinc::Env &e,
                 const std::vector<std::unique_ptr<ToolPass>> &passes,
                 std::ostream &_log);
