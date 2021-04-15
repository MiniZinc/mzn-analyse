#include <string>
#include <vector>

namespace MznData {
void annotate(MiniZinc::EnvI &envi, MiniZinc::Model *model, std::string& output);
void extract(MiniZinc::Model *model, std::string& output);
void objective(MiniZinc::Model *model, std::string& model_output, std::string& termtype_output);
}; // namespace MznData
