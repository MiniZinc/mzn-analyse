#include <iostream>
#include <string>
#include <vector>

#include "passes/annotate_data_deps.hh"
#include "passes/discretise.hh"
#include "passes/filter_items.hh"
#include "passes/get_ast.hh"
#include "passes/get_data_deps.hh"
#include "passes/get_diversity_annotations.hh"
#include "passes/slackify.hh"
#include "passes/get_exprs.hh"
#include "passes/get_items.hh"
#include "passes/get_solve_anns.hh"
#include "passes/get_term_types.hh"
#include "passes/inline_includes.hh"
#include "passes/json_tool.hh"
#include "passes/let_substituter.hh"
#include "passes/output_all.hh"
#include "passes/read_model.hh"
#include "passes/remove_annotations.hh"
#include "passes/remove_includes.hh"
#include "passes/remove_litter.hh"
#include "passes/repeat_model.hh"
#include "passes/write_model.hh"
#include "string_utils.hh"
#include "tool_pass.hh"

using namespace MiniZinc;
using namespace MznAnalyse;

using std::string;
using std::unique_ptr;
using std::vector;

void print_usage() {
  std::cout << " usage:\n"
            << "   mzn-analyse in.mzn [passes...]\n"
            << "\n"
            << " passes:\n"
            << "   in:in.mzn\n"
            << "     Read input file (no support for stdout)\n"
            << "   out:out.mzn\n"
            << "     Write model to out.mzn (- for stdout)\n"
            << "   out_fzn:out.fzn\n"
            << "     Write model to out.fzn (- for stdout)\n"
            << "   no_out\n"
            << "     Disable automatic output insertion\n"
            << "   json_out:out.json\n"
            << "     Write collected json output to out.json (- for stdout)\n"
            << "   json_clear\n"
            << "     Clear collected json output\n"
            << "   no_json\n"
            << "     Disable automatic json output\n"
            << "   inline-includes\n"
            << "     Inline non-library includes\n"
            << "   inline-all-includes\n"
            << "     Inline all includes\n"
            << "   remove-anns:name1,[name2,...]\n"
            << "     Remove Id and Call annotations matching names\n"
            << "   remove-includes:name1,[name2,...]\n"
            << "     Remove includes matching names\n"
            << "   output-all\n"
            << "     Add 'add_to_output' annotation to all VarDecls\n"
            << "   remove-stdlib\n"
            << "     Remove stdlib includes\n"
            << "   remove-litter\n"
            << "     Remove variables and functions that stdlib leaves in the root Model after "
               "typecheck\n"
            << "   remove-litter-aggressive\n"
            << "     Remove variables and functions that stdlib leaves in the root Model after "
               "typecheck\n"
            << "   get-items:idx1,[idx2,...]\n"
            << "     Narrow to items indexed by idx1,...\n"
            << "   filter-items:iid1,[iid2,...]\n"
            << "     Only keep items matching iids\n"
            << "   remove-items:iid1,[iid2,...]\n"
            << "     Remove items matching iids\n"
            << "   filter-typeinst:{var|par}\n"
            << "     Just show var/par parts of model\n"
            << "   replace-with-newvar:location1,location2\n"
            << "     Replace expressions with 'let' expressions\n"
            << "   repeat-model:k\n"
            << "     Make k copies of the model with unique ids (default: 2)\n"
            << "   get-solve-anns\n"
            << "     Get solve annotations\n"
            << "\n"
            << "   slackify\n"
            << "     Use in conjunction with slack.mzn annotations to build relaxed model\n"
            << "   get-diversity-anns\n"
            << "     Extract solution diversity parameters from model\n"
            << "   annotate-data-deps\n"
            << "     Annotate expressions with their data dependencies\n"
            << "   get-term-types:out.terms\n"
            << "     Write .terms file with types of objective terms\n"
            << "   get-data-deps:out.cons (FlatZinc only)\n"
            << "     Write .cons file with data dependenceis of\n"
            << "     FlatZinc constraints\n"
            << "   get-exprs:location1,location2\n"
            << "     Extract list of expressions occurring inside location\n"
            << "     location = path.mzn|sl|sc|el|ec\n"
            << "   get-exprs-types:location1,location2\n"
            << "     Extract list of expressions with their types occurring inside location\n"
            << "     location = path.mzn|sl|sc|el|ec\n"
            << "   get-ast:location1,location2\n"
            << "     Build JSON representation of AST for whole model or just for\n"
            << "     expression matching location1 or location2, place in "
               "json_store\n"
            << "   discretise:base_factor\n"
            << "     Attempt to discretise a MIP flatzinc model\n"
            << "\n";
}

string trimLeadingDash(string arg) {
  int idx = 0;
  while(idx < arg.size() && arg[idx] == '-') { idx++; };
  return arg.substr(idx, arg.size()-idx);
}

struct PassCmd {
  string cmd;
  vector<string> args;

  PassCmd(const string& cmd_str) {
    size_t idx = cmd_str.find(':');
    if (idx == string::npos) {
      cmd = cmd_str;
    } else {
      cmd = cmd_str.substr(0, idx);
      args = utils::split(cmd_str.substr(idx + 1, cmd_str.size()), ',', false);
    }
  }

  PassCmd(const string& c, const string& a_str) : cmd{c} { args = utils::split(a_str, ',', false); }

  MiniZinc::Pass* getPass(std::vector<std::string>& json_store) {
    if (cmd == "inline-includes") {
      return new InlineIncludes();
    } else if (cmd == "inline-all-includes") {
      return new InlineIncludes(false);
    } else if (cmd == "annotate-data-deps") {
      return new AnnotateDataDeps();
    } else if (cmd == "get-term-types") {
      return new GetTermTypes();
    } else if (cmd == "get-solve-anns") {
      return new GetSolveAnns();
    } else if (cmd == "slackify") {
      return new Slackify();
    } else if (cmd == "get-diversity-anns") {
      return new GetDiversityAnns();
    } else if (cmd == "get-data-deps") {
      return new GetDataDeps();
    } else if (cmd == "replace-with-newvar") {
      return new LetSubstituter(args);
    } else if (cmd == "get-exprs") {
      return new GetExprs(args, true);
    } else if (cmd == "get-exprs-types") {
      return new GetExprs(args, true, true);
    } else if (cmd == "remove-anns") {
      return new RemoveAnnotations(args);
    } else if (cmd == "remove-includes") {
      return new RemoveIncludes(args);
    } else if (cmd == "output-all") {
      return new OutputAll;
    } else if (cmd == "remove-stdlibs") {
      return new RemoveIncludes({"solver_redefinitions.mzn", "stdlib.mzn"});
    } else if (cmd == "remove-litter") {
      return new RemoveLitter(false);
    } else if (cmd == "remove-litter-aggressive") {
        return new RemoveLitter(true);
    } else if (cmd == "in") {
      if (args.empty()) {
        return nullptr;
      }
      return new ReadModel(args);
    } else if (cmd == "out") {
      if (args.empty()) {
        args.push_back("-");
      }
      return new WriteModel(args[0], false);
    } else if (cmd == "out_fzn") {
      if (args.empty()) {
        args.push_back("-");
      }
      return new WriteModel(args[0], true);
    } else if (cmd == "repeat-model") {
      unsigned int k = 2;
      if (!args.empty()) {
        int sk = stoi(args[0]);
        if (sk >= 0) {
          k = sk;
        }
      }
      return new RepeatModel(k);
    } else if (cmd == "discretise") {
      unsigned int k = 1;
      if (!args.empty()) {
        int sk = stoi(args[0]);
        if (sk >= 0) {
          k = sk;
        }
      }
      return new Discretise(k);
    } else if (cmd == "json_out") {
      if (args.empty()) {
        args.push_back("-");
      }
      return new JSONTool(json_store, JSONTool::J_Output, args[0]);
    } else if (cmd == "json_clear") {
      return new JSONTool(json_store, JSONTool::J_Clear, "");
    } else if (cmd == "get-ast") {
      return new GetAST(args);
    } else if (cmd == "get-items") {
      vector<size_t> idxs;
      for (const string& idx_str : args) {
        int idx = stoi(idx_str);
        if (idx < 0) return nullptr;
        idxs.push_back(idx);
      }
      return new GetItems(idxs);
    } else if (cmd == "remove-items" || cmd == "filter-items") {
      vector<Item::ItemId> rm_args;
      Item::ItemId iid = Item::II_SOL;
      for (string& item : args) {
        if (item == "include") {
          iid = Item::II_INC;
        } else if (item == "vardecl") {
          iid = Item::II_VD;
        } else if (item == "assign") {
          iid = Item::II_ASN;
        } else if (item == "constraint") {
          iid = Item::II_CON;
        } else if (item == "solve") {
          iid = Item::II_SOL;
        } else if (item == "output") {
          iid = Item::II_OUT;
        } else if (item == "function") {
          iid = Item::II_FUN;
        } else {
          return nullptr;
        }
        rm_args.push_back(iid);
      }
      if (cmd == "remove-items") {
        return new FilterItems(rm_args, true);
      } else if (cmd == "filter-items") {
        return new FilterItems(rm_args, false);
      }
    } else if (cmd == "filter-typeinst") {
      vector<Item::ItemId> keep_args = {Item::II_VD, Item::II_CON};
      if (args.empty()) args.push_back("all");
      if (args[0] == "all") {
        return new FilterItems(keep_args, false, FilterItems::ALL);
      } else if (args[0] == "var") {
        return new FilterItems(keep_args, false, FilterItems::VAR);
      } else if (args[0] == "par") {
        return new FilterItems(keep_args, false, FilterItems::PAR);
      } else {
        return nullptr;
      }
    }
    return nullptr;
  }
};

std::ostream& operator<<(std::ostream& os, const PassCmd& pass) {
  os << pass.cmd << ":" << utils::join(pass.args, ",");
  return os;
}

bool isModelPath(const string& arg) {
  if (arg.size() >= 4) {
    string ext = arg.substr(arg.size() - 4, 4);
    return ext == ".mzn" || ext == ".fzn" || ext == ".dzn";
  }
  return arg == "-";
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Incorrect number of arguments" << std::endl;
    print_usage();
    return EXIT_FAILURE;
  }

  if (std::string(argv[1]) == "help" || std::string(argv[1]) == "--help" ||
      std::string(argv[1]) == "-h") {
    print_usage();
    return EXIT_SUCCESS;
  }
  string in_path = argv[1];

  string out_path;
  string extra_arg;

  bool has_output = false;
  bool has_json_output = false;
  bool no_out = false;
  bool no_json = false;

  bool finished_path_args = false;

  string extension = in_path.size() > 4 ? in_path.substr(in_path.size() - 4, string::npos) : ".mzn";
  bool is_fzn = extension == ".fzn";
  string output_base = in_path.substr(0, in_path.size() - 4);

  // Sub-sections in the pipeline
  // First argument is an implicit "in:" command
  // Collect all passes and non-passes in sub-sections until the next explicit "in:" command
  // Once we encounter an explicit "in:" command, we add "in:" command for collected non-passes,
  // then we add the passes, then we clear our sub-sections.

  // Actual passes pipeline
  vector<unique_ptr<MiniZinc::Pass>> passes;
  // This json_store is used for all sub-sections
  std::vector<std::string> json_store;

  // sub-section pipelines
  vector<string> section_in_paths;
  vector<MiniZinc::Pass*> section_passes;

  for (size_t i = 1; i < argc; i++) {
    vector<string> split_args;

    // Attempt to stitch pass args together (arg ends with : or ,)
    // We expect: pass:arg1,arg2,arg3
    // This should be possible: --pass: arg1, arg2,arg3
    string arg = trimLeadingDash(string(argv[i]));
    split_args.push_back(arg);
    while(arg[arg.size()-1] == ':' || arg[arg.size()-1] == ',') {
      i++;
      if (i >= argc) {
        break;
      }
      arg = string(argv[i]);
      split_args.push_back(arg);
    }

    PassCmd pass_cmd{utils::join(split_args, "")};
    MiniZinc::Pass* pass = pass_cmd.getPass(json_store);
    if (pass) {
      // pass
      if (pass_cmd.cmd == "in") {
        // finish section, start new section
        string paths = utils::join(section_in_paths, ",");
        passes.emplace_back(PassCmd("in", paths).getPass(json_store));
        for (MiniZinc::Pass* p : section_passes) {
          passes.emplace_back(p);
        }
        section_in_paths.clear();
        section_passes.clear();

        section_in_paths.insert(section_in_paths.end(), pass_cmd.args.begin(), pass_cmd.args.end());
        delete pass;
      } else if (pass_cmd.cmd == "out" || pass_cmd.cmd == "out_fzn") {
        has_output = true;
        section_passes.push_back(PassCmd("remove-stdlibs").getPass(json_store));
        section_passes.emplace_back(pass);
      } else if (pass_cmd.cmd == "help" || pass_cmd.cmd == "--help" || pass_cmd.cmd == "-h") {
        delete pass;
        print_usage();
        return EXIT_SUCCESS;
      } else {
        section_passes.push_back(pass);
      }
    } else {
      if (pass_cmd.cmd == "no_out") {
        no_out = true;
        continue;
      } else if (pass_cmd.cmd == "no_json") {
        no_json = true;
        continue;
      } else if (pass_cmd.cmd == "json_out") {
        has_json_output = true;
        continue;
      } else {
        // non-pass
        string path{argv[i]};
        if (!isModelPath(path)) {
          std::cerr << "Unknown filetype: " << path << std::endl;
          print_usage();
          return EXIT_FAILURE;
        }
        section_in_paths.emplace_back(argv[i]);
      }
    }
  }
  if (!(section_passes.empty() && section_in_paths.empty())) {
    // finish section, start new section
    string paths = utils::join(section_in_paths, ",");
    passes.emplace_back(PassCmd("in", paths).getPass(json_store));
    for (MiniZinc::Pass* p : section_passes) {
      passes.emplace_back(p);
    }
    section_in_paths.clear();
    section_passes.clear();
  }

  if (!no_out && !has_output) {
    passes.emplace_back(PassCmd("remove-stdlibs").getPass(json_store));
    passes.emplace_back(PassCmd(is_fzn ? "out_fzn" : "out", "-").getPass(json_store));
  }
  if (!no_json && !has_json_output) {
    passes.emplace_back(PassCmd("json_out").getPass(json_store));
  }

  MiniZinc::GCLock lock;
  MiniZinc::Env env;

  multiPassFlatten(env, passes, json_store, std::cerr);

  return EXIT_SUCCESS;
}
