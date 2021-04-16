#pragma once

#include <minizinc/flatten.hh>
#include <memory>
#include <vector>
#include <ostream>

class ToolPass {
public:
  ToolPass(){};
  virtual MiniZinc::Env* run(MiniZinc::Env* env, std::ostream& log) = 0;
  virtual ~ToolPass(){};
};

MiniZinc::Env* multiPassFlatten(MiniZinc::Env& e,
                                const std::vector<std::unique_ptr<ToolPass> >& passes,
                                std::ostream& _log);

