#include <iostream>
#include <string>
#include <vector>

#include "pass_json_tool.hh"
#include "pass_read_model.hh"
#include "pass_write_model.hh"

#include "pass_annotate_data_deps.hh"

#include "pass_get_data_deps.hh"
#include "pass_get_exprs.hh"
#include "pass_get_term_types.hh"

#include "pass_filter_items.hh"
#include "pass_get_items.hh"

#include "pass_inline_includes.hh"
#include "pass_output_all.hh"
#include "pass_remove_annotations.hh"
#include "pass_remove_includes.hh"

#include "pass_let_substituter.hh"

#include "string_utils.hh"
#include "tool_pass.hh"

using namespace MiniZinc;

using std::string;
using std::unique_ptr;
using std::vector;

void print_usage() {
  std::cout << " usage:\n"
            << "   mzn_tool in.mzn [passes...]\n"
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
            << "\n"
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
            << "\n";
}

struct PassCmd {
  string cmd;
  vector<string> args;

  PassCmd(const string &cmd_str) {
    size_t idx = cmd_str.find(':');
    if (idx == string::npos) {
      cmd = cmd_str;
    } else {
      cmd = cmd_str.substr(0, idx);
      args = utils::split(cmd_str.substr(idx + 1, cmd_str.size()), ',', false);
    }
  }

  PassCmd(const string &c, const string &a_str) : cmd{c} {
    args = utils::split(a_str, ',', false);
  }

  MiniZinc::Pass *getPass(std::vector<std::string> &json_store) {
    if (cmd == "inline-includes") {
      return new InlineIncludes();
    } else if (cmd == "inline-all-includes") {
      return new InlineIncludes(false);
    } else if (cmd == "annotate-data-deps") {
      return new AnnotateDataDeps();
    } else if (cmd == "get-term-types") {
      if (args.empty()) {
        args.push_back("");
      }
      return new GetTermTypes(args[0]);
    } else if (cmd == "get-data-deps") {
      return new GetDataDeps();
    } else if (cmd == "replace-with-newvar") {
      return new LetSubstituter(args);
    } else if (cmd == "get-exprs") {
      return new GetExprs(args);
    } else if (cmd == "remove-anns") {
      return new RemoveAnnotations(args);
    } else if (cmd == "remove-includes") {
      return new RemoveIncludes(args);
    } else if (cmd == "output-all") {
      return new OutputAll;
    } else if (cmd == "remove-stdlibs") {
      return new RemoveIncludes({"solver_redefinitions.mzn", "stdlib.mzn"});
    } else if (cmd == "in") {
      if (args.empty()) {
        return nullptr;
      }
      return new ReadModel(args[0]);
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
    } else if (cmd == "json_out") {
      if (args.empty()) {
        args.push_back("-");
      }
      return new JSONTool(json_store, JSONTool::J_Output, args[0]);
    } else if (cmd == "json_clear") {
      return new JSONTool(json_store, JSONTool::J_Clear, "");
    } else if (cmd == "get-items") {
      vector<size_t> idxs;
      for (const string &idx_str : args) {
        int idx = stoi(idx_str);
        if (idx < 0)
          return nullptr;
        idxs.push_back(idx);
      }
      return new GetItems(idxs);
    } else if (cmd == "remove-items" || cmd == "filter-items") {
      vector<Item::ItemId> rm_args;
      Item::ItemId iid = Item::II_SOL;
      for (string &item : args) {
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
      if (args.empty())
        args.push_back("all");
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

std::ostream &operator<<(std::ostream &os, const PassCmd &pass) {
  os << pass.cmd << ":" << utils::join(pass.args, ",");
  return os;
}

int main(int argc, char **argv) {

  if (argc < 2) {
    std::cerr << "Incorrect number of arguments" << std::endl;
    print_usage();
    return EXIT_FAILURE;
  }

  string in_path = argv[1];

  string out_path;
  string extra_arg;

  bool has_output = false;
  bool has_json_output = false;
  bool no_out = false;
  bool no_json = false;

  string extension = in_path.substr(in_path.size() - 4, string::npos);
  bool is_fzn = extension == ".fzn";
  string output_base = in_path.substr(0, in_path.size() - 4);

  vector<PassCmd> pass_cmdline;
  pass_cmdline.emplace_back("in", in_path);
  for (size_t i = 2; i < argc; i++) {
    PassCmd pass{string(argv[i])};
    if (pass.cmd == "no_out") {
      no_out = true;
      continue;
    }
    if (pass.cmd == "no_json") {
      no_json = true;
      continue;
    }
    if (pass.cmd == "out" || pass.cmd == "out_fzn") {
      has_output = true;
      pass_cmdline.emplace_back("remove-stdlibs");
    }
    if (pass.cmd == "json_out") {
      has_json_output = true;
    }
    pass_cmdline.push_back(pass);
  }
  if (!no_out && !has_output) {
    pass_cmdline.emplace_back("remove-stdlibs");
    pass_cmdline.emplace_back(is_fzn ? "out_fzn" : "out", "-");
  }
  if (!no_json && !has_json_output) {
    pass_cmdline.emplace_back("json_out");
  }

  // Build actual passes pipeline
  std::vector<std::string> json_store;
  vector<unique_ptr<MiniZinc::Pass>> passes;
  for (PassCmd &pass : pass_cmdline) {
    Pass *pass_ptr = pass.getPass(json_store);
    if (pass_ptr == nullptr) {
      std::cerr << "Cannot process pass: " << pass << std::endl;
      return EXIT_FAILURE;
    }
    passes.emplace_back(pass_ptr);
  }

  GCLock lock;
  Env env;

  Env *out_env = multiPassFlatten(env, passes, json_store, std::cerr);

  return EXIT_SUCCESS;
}
