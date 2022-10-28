#include "passes/get_diversity_annotations.hh"

#include <fstream>
#include <iostream>
#include <minizinc/astiterator.hh>
#include <minizinc/eval_par.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/flatten.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>
#include <minizinc/type.hh>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "string_utils.hh"

using namespace MiniZinc;
using std::ostream;
using std::string;
using std::unordered_map;
using std::vector;

namespace MznTool {

void GetDiversityAnns::collect_diversity_annotations(MiniZinc::Env* env, MiniZinc::Model* m) {
  // Get SolveI si
  SolveI* si = m->solveItem();
  if (!si) return;

  // Build div_opt.objective
  if (si->st() == SolveI::ST_SAT) {
    // No objective / SAT problem
    div_opts.objective.name = "";
    div_opts.objective.type = "";
    div_opts.objective.sense = 0;
  } else {
    //   Construct temporary div_obj = si->e
    const string obj_name = "div_orig_objective";

    TypeInst* ti = nullptr;
    Expression* e = si->e();
    if (Id* id = e->dynamicCast<Id>()) {
      VarDecl* typed = id->decl();
      ti = typed->ti();
    } else {
      ti = new TypeInst(Location().introduce(), e->type());
    }

    VarDecl* objVd = new VarDecl(Location().introduce(), ti, obj_name, e);
    objVd->ann().add(MiniZinc::Constants::constants().ann.output);
    m->addItem(VarDeclI::a(Location().introduce(), objVd));

    div_opts.objective.name = obj_name;
    std::stringstream ss;
    ss << *ti;
    div_opts.objective.type = ss.str();
    div_opts.objective.sense = si->st() == SolveI::ST_MIN ? -1 : 1;
  }

  // Mark si as removed
  si->remove();

  // Collect annotations
  Annotation& anns = si->ann();
  for (Expression* ann_e : anns) {
    try {
      Expression* e = eval_par(env->envi(), ann_e);

      if (Call* ca = e->dynamicCast<Call>()) {
        if (ca->id() == string("diversity_inter_constraint") && ca->argCount() == 1) {
          div_opts.inter_diversity_constraint = eval_string(env->envi(), ca->arg(0));
        } else if (ca->id() == string("diversity_intra_constraint") && ca->argCount() == 1) {
          div_opts.intra_diversity_constraint = eval_string(env->envi(), ca->arg(0));
        } else if (ca->id() == string("diversity_aggregator") && ca->argCount() == 1) {
          div_opts.aggregator = eval_string(env->envi(), ca->arg(0));
        } else if (ca->id() == string("diversity_combinator") && ca->argCount() == 1) {
          std::stringstream ss;
          ss << *ca->arg(0);
          div_opts.combinator = eval_string(env->envi(), ca->arg(0));
        } else if (ca->id() == string("diversity_incremental") && ca->argCount() == 2) {
          div_opts.type = "incremental";
          div_opts.k = eval_int(env->envi(), ca->arg(0)).toInt();
          div_opts.gap = eval_float(env->envi(), ca->arg(1)).toDouble();
        } else if (ca->id() == string("diversity_global") && ca->argCount() == 2) {
          div_opts.type = "global";
          div_opts.k = eval_int(env->envi(), ca->arg(0)).toInt();
          div_opts.gap = eval_float(env->envi(), ca->arg(1)).toDouble();
        } else if (ca->id() == string("diversity_pairwise") && ca->argCount() == 2) {
          VarInfo vi;

          std::stringstream varname_ss;
          varname_ss << "div_curr_var_" << div_opts.vars.size();
          const string varname = varname_ss.str();

          TypeInst* ti;
          Expression* arg0 = ca->arg(0);
          VarDecl* arg_vd = ca->decl()->param(0);

          if (Id* id = arg0->dynamicCast<Id>()) {
            VarDecl* typed = id->decl();
            ti = typed->ti();
          } else {
            ti = new TypeInst(Location().introduce(), arg_vd->type(), arg_vd->ti()->ranges(),
                              arg_vd->ti()->domain());
          }

          VarDecl* newVar = new VarDecl(Location().introduce(), ti, varname, arg0);
          newVar->ann().add(MiniZinc::Constants::constants().ann.output);
          m->addItem(VarDeclI::a(Location().introduce(), newVar));
          vi.name = varname;

          std::stringstream type_ss;
          type_ss << *ti;
          vi.type = type_ss.str();

          // Previous array

          std::stringstream prevvarname_ss;
          prevvarname_ss << "div_prev_var_" << div_opts.vars.size();
          const string prevvarname = prevvarname_ss.str();

          auto ranges = ti->ranges();
          vector<TypeInst*> prev_ranges;
          prev_ranges.push_back(new TypeInst(Location().introduce(), Type::parint()));
          for (int i = 0; i < ranges.size(); i++) {
            prev_ranges.push_back(ranges[i]);
          }

          TypeInst* prev_ti = new TypeInst(Location().introduce(), arg_vd->type(), prev_ranges,
                                           arg_vd->ti()->domain());
          VarDecl* prevVar = new VarDecl(Location().introduce(), prev_ti, prevvarname);
          prevVar->ann().add(MiniZinc::Constants::constants().ann.output);
          m->addItem(VarDeclI::a(Location().introduce(), prevVar));
          vi.prev_name = prevvarname;

          std::stringstream prev_type_ss;
          prev_type_ss << *prev_ti;
          vi.prev_type = prev_type_ss.str();

          vi.distance_function = eval_string(env->envi(), ca->arg(1));

          div_opts.vars.emplace_back(vi);
        }
      }
    } catch (const LocationException& e) {
      std::cerr << e.loc() << ":" << std::endl;
      std::cerr << e.what() << ": " << e.msg() << std::endl;
    } catch (const Exception& e) {
      std::string what = e.what();
      std::cerr << what << (what.empty() ? "" : ": ") << e.msg() << std::endl;
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    } catch (...) {
      std::cerr << "  UNKNOWN EXCEPTION." << std::endl;
    }
  }
}

void GetDiversityAnns::write_json(ostream& os) {
  os << "{\n"
     << "  \"div_type\": \"" << div_opts.type << "\",\n"
     << "  \"k\": " << div_opts.k << ",\n"
     << "  \"gap\": " << div_opts.gap << ",\n"
     << "  \"inter_diversity_constraint\": \"" << div_opts.inter_diversity_constraint << "\",\n"
     << "  \"intra_diversity_constraint\": \"" << div_opts.intra_diversity_constraint << "\",\n"
     << "  \"aggregator\": \"" << div_opts.aggregator << "\",\n"
     << "  \"combinator\": \"" << div_opts.combinator << "\",\n"
     << "  \"objective\": {\n"
     << "    \"name\": \"" << div_opts.objective.name << "\",\n"
     << "    \"type\": \"" << div_opts.objective.type << "\",\n"
     << "    \"sense\": \"" << div_opts.objective.sense << "\"\n"
     << "  },\n"
     << "  \"vars\": [\n";

  std::vector<std::string> var_specs;
  for (auto& vi : div_opts.vars) {
    std::stringstream ss;
    ss << "    {\n"
       << "      \"name\": \"" << vi.name << "\",\n"
       << "      \"type\": \"" << vi.type << "\",\n"
       << "      \"prev_name\": \"" << vi.prev_name << "\",\n"
       << "      \"prev_type\": \"" << vi.prev_type << "\",\n"
       << "      \"distance_function\": \"" << vi.distance_function << "\"\n"
       << "    }";
    var_specs.emplace_back(ss.str());
  }
  os << utils::join(var_specs, ",\n");
  os << "\n  ]\n";
  os << "}\n";
}

GetDiversityAnns::GetDiversityAnns() {}

std::string GetDiversityAnns::get_name() { return "get-diversity-annotations"; }

MiniZinc::Env* GetDiversityAnns::run(MiniZinc::Env* e, std::ostream& log) {
  Model* m = e->model();

  div_opts.k = 1;
  div_opts.gap = 0.00;
  div_opts.type = "iterative";
  div_opts.objective.name = "";
  div_opts.objective.sense = 0;
  div_opts.inter_diversity_constraint = "";
  div_opts.intra_diversity_constraint = "";
  div_opts.aggregator = "";
  div_opts.combinator = "";

  collect_diversity_annotations(e, m);

  return e;
}

};  // namespace MznTool
