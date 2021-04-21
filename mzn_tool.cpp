#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "pass_annotate_data_deps.hh"
#include "pass_filter_items.hh"
#include "pass_get_data_deps.hh"
#include "pass_get_items.hh"
#include "pass_get_term_types.hh"
#include "pass_inline_includes.hh"
#include "pass_read_model.hh"
#include "pass_remove_annotations.hh"
#include "pass_remove_includes.hh"
#include "pass_write_model.hh"
#include "tool_pass.hh"

#include <minizinc/file_utils.hh>

using namespace MiniZinc;

using std::string;
using std::unique_ptr;
using std::vector;

string join(const vector<string> &strs, const string &sep) {
  std::stringstream ss;
  for (size_t i = 0; i < strs.size(); i++) {
    if (i)
      ss << sep;
    ss << strs[i];
  }
  return ss.str();
}

vector<string> split(const string &str, char delim, bool include_empty) {
  std::stringstream ss;
  ss.str(str);
  std::string item;

  vector<string> result;

  auto inserter = std::back_inserter(result);

  while (std::getline(ss, item, delim)) {
    if (!item.empty() || include_empty)
      *(inserter++) = item;
  }

  return result;
}

void print_usage() {
  std::cout << " usage:\n"
            << "   mzn_tool  sequence     in [passes...]\n"
            << "   mzn_tool  annotate in.mzn [out.mzn]\n"
            << "   mzn_tool get_terms in.mzn [out.mzn] [out.terms]\n"
            << "   mzn_tool  get_data in.fzn [out.fzn]  [out.cons]\n";

  std::cout << "\n"
            << "   annotate => inline-includes\n"
            << "               annotate-data-deps\n"
            << "   get_terms => get-term-types:out.terms\n"
            << "                remove-anns data\n"
            << "                remove-items:solve,output\n"
            << "   get_data => get-data-deps:out.cons\n"
            << "               remove-anns:data\n";

  std::cout << "\n"
            << " passes:\n"
            << "   in:in.mzn\n"
            << "     Read input file (no support for stdout)\n"
            << "   out:out.mzn\n"
            << "     Write model to out.mzn (- for stdout)\n"
            << "   out_fzn:out.fzn\n"
            << "     Write model to out.fzn (- for stdout)\n"
            << "   no_out\n"
            << "     Disable automatic output insertion\n"
            << "   inline-includes\n"
            << "     Inline non-library includes\n"
            << "   inline-all-includes\n"
            << "     Inline all includes\n"
            << "   remove-anns:name1,[name2,...]\n"
            << "     Remove Id and Call annotations matching names\n"
            << "   remove-includes:name1,[name2,...]\n"
            << "     Remove includes matching names\n"
            << "   remove-stdlib\n"
            << "     Remove stdlib includes\n"
            << "   get-items:idx1,[idx2,...]\n"
            << "     Narrow to items indexed by idx1,...\n"
            << "   filter-items:iid1,[iid2,...]\n"
            << "     Only keep items matching iids\n"
            << "   remove-items:iid1,[iid2,...]\n"
            << "     Remove items matching iids\n"
            << "\n"
            << "   annotate-data-deps\n"
            << "     Annotate expressions with their data dependencies\n"
            << "   get-term-types:out.terms\n"
            << "     Write .terms file with types of objective terms\n"
            << "   get-data-deps:out.cons (FlatZinc only)\n"
            << "     Write .cons file with data dependenceis of\n"
            << "     FlatZinc constraints\n"
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
      args = split(cmd_str.substr(idx + 1, cmd_str.size()), ',', false);
    }
  }

  PassCmd(const string &c, const string &a_str) : cmd{c} {
    args = split(a_str, ',', false);
  }

  MiniZinc::Pass *getPass() {
    if (cmd == "inline-includes") {
      return new InlineIncludes();
    } else if (cmd == "inline-all-includes") {
      return new InlineIncludes(false);
    } else if (cmd == "annotate-data-deps") {
      return new AnnotateDataDeps();
    } else if (cmd == "get-term-types") {
      if (args.empty()) {
        args.push_back("-");
      }
      return new GetTermTypes(args[0]);
    } else if (cmd == "get-data-deps") {
      if (args.empty()) {
        args.push_back("-");
      }
      return new GetDataDeps(args[0]);
    } else if (cmd == "remove-anns") {
      return new RemoveAnnotations(args);
    } else if (cmd == "remove-includes") {
      return new RemoveIncludes(args);
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
    }
    return nullptr;
  }
};

std::ostream &operator<<(std::ostream &os, const PassCmd &pass) {
  os << pass.cmd << ":" << join(pass.args, ",");
  return os;
}

int main(int argc, char **argv) {

  if (argc < 3) {
    std::cerr << "Incorrect number of arguments" << std::endl;
    print_usage();
    return EXIT_FAILURE;
  }

  string cmd = argv[1];
  string in_path = argv[2];

  string out_path;
  string extra_arg;

  bool has_output = false;
  bool no_out = false;

  string extension = in_path.substr(in_path.size() - 4, string::npos);
  bool is_fzn = extension == ".fzn";
  string output_base = in_path.substr(0, in_path.size() - 4);

  vector<PassCmd> pass_cmdline;
  pass_cmdline.emplace_back("in", in_path);
  if (cmd == "sequence") {
    for (size_t i = 3; i < argc; i++) {
      PassCmd pass{string(argv[i])};
      if (pass.cmd == "no_out") {
        no_out = true;
        continue;
      }
      if (pass.cmd == "out" || pass.cmd == "out_fzn") {
        has_output = true;
        pass_cmdline.emplace_back("remove-stdlibs");
      }
      pass_cmdline.push_back(pass);
    }
  } else if (cmd == "annotate") {
    if (argc > 3)
      out_path = argv[3];
    if (out_path.empty()) {
      out_path = output_base + ".annotated.mzn";
    }
    pass_cmdline.emplace_back("inline-includes");
    pass_cmdline.emplace_back("annotate-data-deps");
    pass_cmdline.emplace_back("remove-stdlibs");
    pass_cmdline.emplace_back("out", out_path);
    has_output = true;
  } else if (cmd == "get_terms") {
    if (argc > 3)
      out_path = argv[3];
    if (argc > 4)
      extra_arg = argv[4];

    if (out_path.empty()) {
      out_path = output_base + ".solveless.mzn";
    }
    if (extra_arg.empty()) {
      extra_arg = output_base + ".terms";
    }
    pass_cmdline.emplace_back("get-term-types", extra_arg);
    pass_cmdline.emplace_back("remove-anns", "data");
    pass_cmdline.emplace_back("remove-items", "solve,output");
    pass_cmdline.emplace_back("remove-stdlibs");
    pass_cmdline.emplace_back("out", out_path);
    has_output = true;
  } else if (cmd == "get_data") {
    if (!is_fzn) {
      std::cerr << "get_data must take a fzn file as input" << std::endl;
      print_usage();
      return EXIT_FAILURE;
    }
    if (argc > 3)
      out_path = argv[3];
    if (argc > 4)
      extra_arg = argv[4];
    if (out_path.empty()) {
      out_path = output_base + ".noanns.fzn";
    }
    if (extra_arg.empty()) {
      extra_arg = output_base + ".cons";
    }
    pass_cmdline.emplace_back("get-data-deps", extra_arg);
    pass_cmdline.emplace_back("remove-anns", "data");
    pass_cmdline.emplace_back("remove-stdlibs");
    pass_cmdline.emplace_back("out_fzn", out_path);
    has_output = true;
  } else {
    std::cerr << "Unknown command: " << cmd << std::endl;
    print_usage();
    return EXIT_FAILURE;
  }
  if (!no_out && !has_output) {
    pass_cmdline.emplace_back("remove-stdlibs");
    pass_cmdline.emplace_back(is_fzn ? "out_fzn" : "out", "-");
  }

  // Build actual passes pipeline
  vector<unique_ptr<MiniZinc::Pass>> passes;
  for (PassCmd &pass : pass_cmdline) {
    Pass *pass_ptr = pass.getPass();
    if (pass_ptr == nullptr) {
      std::cerr << "Cannot process pass: " << pass << std::endl;
      return EXIT_FAILURE;
    }
    passes.emplace_back(pass_ptr);
  }

  GCLock lock;
  Env env;
  Env *out_env = multiPassFlatten(env, passes, std::cerr);

  return EXIT_SUCCESS;
}
