#pragma once

#include <minizinc/ast.hh>
#include <ostream>
#include <string>
#include <vector>

#include "location_utils.hh"
#include "tool_pass.hh"

namespace MznAnalyse {

typedef std::unordered_map<std::string, std::vector<MiniZinc::Expression*>> LocExprMap;

struct ExprInfo {
  std::string repr;
  std::string type;
  ExprInfo(std::string r, std::string t): repr{r}, type{t} {}
};

struct UniqueCollector {
  std::vector<ShortLoc> locs;
  std::unordered_map<std::string, std::unordered_map<std::string, ExprInfo>> exprs;
  bool include_types;

  UniqueCollector(const std::vector<ShortLoc>& locations, bool typed);
  UniqueCollector(const std::vector<std::string>& paths, bool typed);

  void add_expr(const ShortLoc& loc, MiniZinc::Expression* e);

  void write_json(std::ostream& os);
};

struct ExpressionExtractorEVisitor : public MiniZinc::EVisitor {
  std::vector<ShortLoc> locs;
  LocExprMap& exprs;
  bool collect_par;
  bool no_sub_exprs;

  ExpressionExtractorEVisitor(const std::vector<ShortLoc>& locations, LocExprMap& expr_store,
                              bool only_par = true, bool only_top = false);
  bool enter(MiniZinc::Expression* e);
};

struct ExpressionExtractor : public MiniZinc::ItemVisitor {
  ExpressionExtractorEVisitor eev;

  ExpressionExtractor(const std::vector<ShortLoc>& locations, LocExprMap& expr_store,
                      bool only_par = true, bool only_top = false);

  bool enter(MiniZinc::Item* item);
  void vVarDeclI(MiniZinc::VarDeclI* vdi);
  void vAssignI(MiniZinc::AssignI* ai);
  void vConstraintI(MiniZinc::ConstraintI* ci);
  void vSolveI(MiniZinc::SolveI* si);
  void vFunctionI(MiniZinc::FunctionI* fi);
};

LocExprMap get_exprs(const std::vector<ShortLoc>& locs, MiniZinc::Model* m, bool only_par = true,
                     bool only_top = false);

class GetExprs : public ToolPass {
private:
  UniqueCollector uc;
  bool collect_par;
  bool include_types;

public:
  GetExprs(const std::vector<std::string>& paths, bool only_par = true, bool typed = false);

  MiniZinc::Env* run(MiniZinc::Env* e, std::ostream& log) override;

  void write_json(std::ostream& os) override;
  std::string get_name() override;
};
};  // namespace MznAnalyse
