#pragma once

#include "location_utils.hh"
#include "tool_pass.hh"
#include <minizinc/ast.hh>

#include <ostream>
#include <string>
#include <vector>

namespace MznTool {

class ExprPrinter {

private:
  bool hide_locations;
  bool hide_annotations;
  bool hide_types;

public:
  // ExprPrinter(bool hide_locs = true, bool hide_anns = true);
  ExprPrinter(bool hide_locs = true, bool hide_anns = true, bool hide_types = true);

  std::string to_string(const MiniZinc::Location &loc);
  std::string to_string(const MiniZinc::ASTString &as);
  std::string to_string(const MiniZinc::Annotation &anns);
  std::string to_string(const MiniZinc::Type &type);
  std::string to_string(const MiniZinc::Expression *e);

  void vIntLit(const MiniZinc::IntLit *il, std::vector<std::string> &records);
  void vFloatLit(const MiniZinc::FloatLit *fl,
                 std::vector<std::string> &records);
  void vBoolLit(const MiniZinc::BoolLit *bl, std::vector<std::string> &records);
  void vSetLit(const MiniZinc::SetLit *sl, std::vector<std::string> &records);
  void vStringLit(const MiniZinc::StringLit *sl,
                  std::vector<std::string> &records);
  void vId(const MiniZinc::Id *ident, std::vector<std::string> &records);
  void vAnonVar(const MiniZinc::AnonVar * /*x*/,
                std::vector<std::string> &records);
  void vArrayLit(const MiniZinc::ArrayLit *al,
                 std::vector<std::string> &records);
  void vArrayAccess(const MiniZinc::ArrayAccess *aa,
                    std::vector<std::string> &records);
  void vComprehension(const MiniZinc::Comprehension *comp,
                      std::vector<std::string> &records);
  void vComprehensionGenerator(const MiniZinc::Comprehension * /*c*/,
                               int /*gen_i*/,
                               std::vector<std::string> &records);
  void vITE(const MiniZinc::ITE *ite, std::vector<std::string> &records);
  void vBinOp(const MiniZinc::BinOp *bo, std::vector<std::string> &records);
  void vUnOp(const MiniZinc::UnOp *uo, std::vector<std::string> &records);
  void vCall(const MiniZinc::Call *ca, std::vector<std::string> &records);
  void vLet(const MiniZinc::Let *let, std::vector<std::string> &records);
  void vVarDecl(const MiniZinc::VarDecl *vd, std::vector<std::string> &records);
  void vTypeInst(const MiniZinc::TypeInst *ti,
                 std::vector<std::string> &records);
  void vTIId(const MiniZinc::TIId * /*tiid*/,
             std::vector<std::string> &records);
};

class ASTCollector : MiniZinc::ItemVisitor {
private:
  // Store whole items if no arguments were provided to GetAST
  std::vector<std::string> items;

  // Store mapping { loc1 -> [ast1, ast2], loc2 -> [ast3], ... }
  std::unordered_map<std::string, std::vector<std::string>> asts;

  ExprPrinter ep;

public:
  std::vector<ShortLoc> locs;

  ASTCollector();
  ASTCollector(const std::vector<std::string> &paths);

  static bool enterModel(MiniZinc::Model * /*m*/);
  bool enter(MiniZinc::Item * /*m*/);
  void vIncludeI(MiniZinc::IncludeI *ii);
  void vVarDeclI(MiniZinc::VarDeclI * /*vdi*/);
  void vAssignI(MiniZinc::AssignI * /*ai*/);
  void vConstraintI(MiniZinc::ConstraintI * /*ci*/);
  void vSolveI(MiniZinc::SolveI * /*si*/);
  void vOutputI(MiniZinc::OutputI * /*oi*/);
  void vFunctionI(MiniZinc::FunctionI * /*fi*/);

  void add_ast(const ShortLoc &loc, std::string ast);
  void add_expr(const ShortLoc &loc, MiniZinc::Expression *e);
  void write_json(std::ostream &os);
};

class GetAST : public ToolPass {
private:
  ASTCollector ic;

public:
  GetAST(const std::vector<std::string> &paths);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;

  void write_json(std::ostream &os) override;
  std::string get_name() override;
};
}; // namespace MznTool
