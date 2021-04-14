#include <vector>
#include <string>

namespace MznData {
  void annotate(MiniZinc::EnvI& envi, MiniZinc::Model* model);
  void extract(MiniZinc::Model* model);
  void objective(MiniZinc::Model* model);
};
