#include "passes/json_tool.hh"

#include <fstream>

#include "string_utils.hh"

namespace MznAnalyse {

JSONTool::JSONTool(std::vector<std::string>& store, JSONCommand cmd, const std::string& out)
    : json_store{store}, command{cmd}, out_path{out} {}

void write_to_os(std::ostream& os, std::vector<std::string>& store) {
  if (!store.empty()) {
    os << "{\n";
    os << utils::join(store, ",\n");
    os << "}" << std::endl;
  }
}

MiniZinc::Env* JSONTool::run(MiniZinc::Env* e, std::ostream& log) {
  if (command == J_Output) {
    if (out_path == "-") {
      write_to_os(std::cout, json_store);
    } else {
      std::ofstream out_json_os{out_path};
      write_to_os(out_json_os, json_store);
      out_json_os.close();
    }
  } else if (command == J_Clear) {
    json_store.clear();
  }

  return e;
}

};  // namespace MznAnalyse
