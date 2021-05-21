#include "tool_pass.hh"

#include <minizinc/file_utils.hh>
#include <minizinc/flatten.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>
#include <minizinc/timer.hh>

using MiniZinc::Env;
using MiniZinc::Timer;

Env *multiPassFlatten(
    Env &e, const std::vector<std::unique_ptr<MiniZinc::Pass>> &passes,
    std::vector<std::string> &json_store, std::ostream &_log) {
  Env *pre_env = &e;
  size_t npasses = passes.size();
  pre_env->envi().finalPassNumber = static_cast<unsigned int>(npasses);
  Timer starttime;
  bool verbose = false;
  for (unsigned int i = 0; i < passes.size(); i++) {
    pre_env->envi().currentPassNumber = i;
    if (verbose) {
      _log << "Start pass " << i << ":\n";
    }

    Env *out_env = passes[i]->run(pre_env, _log);
    if (out_env == nullptr) {
      return nullptr;
    }

    // Collect json output
    if (ToolPass *tp = dynamic_cast<ToolPass *>(passes[i].get())) {
      std::stringstream tool_out_stream;
      tp->write_json(tool_out_stream);
      std::string tool_output = tool_out_stream.str();

      if (!tool_output.empty()) {
        std::stringstream ss;
        ss << "\"" << tp->get_name() << "\": " << tool_output;
        json_store.push_back(ss.str());
      }
    }

    if (pre_env != &e && pre_env != out_env) {
      delete pre_env;
    }
    pre_env = out_env;

    if (verbose) {
      _log << "Finish pass " << i << ": " << starttime.stoptime() << "\n";
    }
  }

  return pre_env;
}
