#include "passes/repeat_model.hh"

#include <fstream>
#include <iterator>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/copy.hh>
#include <string>
#include "string_utils.hh"
#include <sstream>

using namespace MiniZinc;
using std::string;
using std::vector;
using std::ostream;

namespace MznTool {


RepeatModel::RepeatModel(unsigned int k) : k_models{k} { }

void RepeatModel::write_json(ostream& os) {
  os << "{\n"
     << "  \"variables\": {\n";

  vector<string> entries;
  for(VdInfo& vdinfo : vds) {
    std::stringstream ss;
    ss << "    \"" << vdinfo.name << "\": [" << utils::join(vdinfo.renames, ",") << "]";
    entries.emplace_back(ss.str());
  }

  os << utils::join(entries, ",\n") << "\n"
     << "  }\n"
     << "}\n";
}

Env* RepeatModel::run(Env* e, std::ostream& log) {
  if(k_models == 0) {
    return e;
  }

  Model* m = e->model();
  for (VarDeclI& vdi : m->vardecls()) {
    VarDecl* vd = vdi.e();
    string name = vd->id()->str().c_str();

    vds.emplace_back(name, vd->id());
  }

  vector<Model*> models;

  for(unsigned int i=0; i<k_models; i++) {
    for(VdInfo& vdinfo : vds) {
      std::stringstream ss;
      ss << vdinfo.name << "_copy_" << i;
      string newname = ss.str();
      vdinfo.id->v(newname);
      vdinfo.renames.push_back(newname);
    }

    Model* m_copy = copy(e->envi(), m);
    models.push_back(m_copy);
  }

  Model* model_0 = models[0];
  for(unsigned int i=1; i<k_models; i++) {
    for(VarDeclI& vdi : models[i]->vardecls()) {
      if(vdi.e()->id()->str() != "output")
        model_0->addItem(&vdi);
    }
    for(ConstraintI& ci : models[i]->constraints()) {
      model_0->addItem(&ci);
    }
  }

  e->model(models[0]);

  return e;
}

};  // namespace MznTool
