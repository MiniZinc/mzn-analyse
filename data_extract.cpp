#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include <minizinc/model.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/solver.hh>
#include <minizinc/astiterator.hh>
#include <minizinc/prettyprinter.hh>

using namespace MiniZinc;
using std::string;
using std::vector;
using std::ostream;

ostream& operator<<(ostream& os, vector<Call*>& calls) {
  if(calls.empty()) {
    os << "[]";
  } else {
    os << "[\n";
    for(int i=0; i<calls.size(); i++) {
      Call* ca = calls[i];
      int depth = ca->arg(0)->cast<IntLit>()->v().toInt();
      string type = ca->arg(1)->cast<StringLit>()->v().c_str();

      os << "    {"
        << "\"depth\": " << depth << ", "
        << "\"type\": \"" << type << "\", ";

      if(type == "lit") {
        os << "\"lit\": " << *ca->arg(2);
      } else if(type == "if") {
        os << "\"if\": " << *ca->arg(2);
      } else if(type == "eq") {
        os << "\"id\": " << *ca->arg(2);
        os << ", \"val\": " << *ca->arg(3);
      } else if(type == "in") {
        os << "\"id\": " << *ca->arg(2);
        os << ", \"index_set\": " << *ca->arg(3);
      } else {
        std::cerr << "UNKNOWN ANNOTATION: " << *ca << std::endl;
      }

      os << "}" << (i != calls.size()-1 ? "," : "") << (i != calls.size()-1 ? "\n" : "");
    }
    os << "]";
  }
  return os;
}

int main(int argc, char**argv) {
  GCLock lock;
  if(argc < 2) {
    std::cerr << argv[0] << ": Invalid arguments." << std::endl;
    std::cerr << "Usage: " << argv[0] << " <mzn>" << std::endl;
    return EXIT_FAILURE;
  }

  vector<string> includes;
  string mzn_stdlib_dir = FileUtils::share_directory();
  includes.push_back(mzn_stdlib_dir + "/std/");

  string fzn_path = argv[1];
  string output_base = fzn_path.substr(0, fzn_path.size() - 4);

  Env env;
  Model* m = parse(env, {fzn_path}, {}, "", "", includes, false, false, false, false, std::cerr);
  if(!m) {
    std::cerr << argv[0] << ": Failed to parse file" << std::endl;
    return EXIT_FAILURE;
  }

  // Collect and write data entries
  string out_json = output_base + ".cons";
  std::cerr << "Writing constraint data to: " << out_json << std::endl;
  std::ofstream out_json_os {out_json};
  out_json_os << "{\n";

  bool first = true;
  size_t con_id = 0;
  for(ConstraintI& ci : m->constraints()) {
    vector<Call*> entries;

    if(first) {
      first = false;
    } else {
      out_json_os << ",\n";
    }

    for(Call* ca = ci.e()->ann().getCall(ASTString("data"));
        ca != nullptr;
        ca = ci.e()->ann().getCall(ASTString("data"))) {
      entries.push_back(ca);
      ci.e()->ann().remove(ca);
    }

    out_json_os << "  \"" << con_id << "\": " << entries;

    con_id++;
  }

  out_json_os << "}" << std::endl;
  out_json_os.close();

  // Write clean fzn file
  string out_fzn = output_base + ".clean.fzn";
  std::cerr << "Writing clean fzn to: " << out_fzn << std::endl;
  std::ofstream out_fzn_os {out_fzn};
  Printer p(out_fzn_os, 0);
  p.print(m);
  out_fzn_os.close();

  return EXIT_SUCCESS;
}
