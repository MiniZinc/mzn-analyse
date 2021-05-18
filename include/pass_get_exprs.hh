#pragma once

#include "tool_pass.hh"
#include "location_utils.hh"
#include <minizinc/ast.hh>

#include <string>
#include <vector>
#include <ostream>

struct UniqueCollector {
  std::vector<ShortLoc> locs;
  std::unordered_map<std::string, std::unordered_set<std::string> > exprs;

  UniqueCollector(const std::vector<ShortLoc>& locations);
  UniqueCollector(const std::vector<std::string>& paths);

  void add_expr(const ShortLoc& loc, std::string s);
  void add_expr(const ShortLoc& loc, MiniZinc::Expression *e);

  void write_json(std::ostream &os);
};

class GetExprs : public ToolPass {
private:
  UniqueCollector uc;

public:
  GetExprs(const std::vector<std::string>& paths);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;

  void write_json(std::ostream &os);
};
