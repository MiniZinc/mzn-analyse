#pragma once

#include <memory>
#include <minizinc/flatten.hh>
#include <ostream>
#include <vector>

MiniZinc::Env *
multiPassFlatten(MiniZinc::Env &e,
                 const std::vector<std::unique_ptr<MiniZinc::Pass>> &passes,
                 std::ostream &_log);
