#pragma once

#include "location_utils.hh"
#include "tool_pass.hh"
#include <minizinc/ast.hh>

#include <ostream>
#include <string>
#include <vector>

class ItemCollector : MiniZinc::ItemVisitor {
  private:

    std::vector<std::string> items;

  public:
    ItemCollector();

    static bool enterModel(MiniZinc::Model* /*m*/);
    static bool enter(MiniZinc::Item* /*m*/);
    void vIncludeI(MiniZinc::IncludeI* ii);
    void vVarDeclI(MiniZinc::VarDeclI* /*vdi*/);
    void vAssignI(MiniZinc::AssignI* /*ai*/);
    void vConstraintI(MiniZinc::ConstraintI* /*ci*/);
    void vSolveI(MiniZinc::SolveI* /*si*/);
    void vOutputI(MiniZinc::OutputI* /*oi*/);
    void vFunctionI(MiniZinc::FunctionI* /*fi*/);

    void write_json(std::ostream &os);
};

class GetAST : public ToolPass {
  private:
    ItemCollector ic;

  public:
    GetAST();

    MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;

    void write_json(std::ostream &os) override;
    std::string get_name() override;
};
