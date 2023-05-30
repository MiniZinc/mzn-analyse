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


std::string RepeatModel::get_name() { return "repeat-model"; }

RepeatModel::RepeatModel(unsigned int k) : k_models{k} { }

void RepeatModel::write_json(ostream& os) {
  os << "{\n"
     << "  \"variables\": {\n";

  vector<string> entries;
  for(VdInfo& vdinfo : vds) {
    std::stringstream ss;
    if(!vdinfo.renames.empty()) {
    ss << "    \"" << vdinfo.name << "\": [\"" << utils::join(vdinfo.renames, "\", \"") << "\"]";
    } else {
      // This shouldn't really be possible
      ss << "    \"" << vdinfo.name << "\": []";
    }
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
    if(vd->type().ti() == MiniZinc::Type::TI_VAR) {
      string name = vd->id()->str().c_str();
      vds.emplace_back(name, vd->id());
    }
  }

  vector<Model*> models;

  for(unsigned int i=0; i<k_models; i++) {
    for(VdInfo& vdinfo : vds) {
      std::stringstream ss;
      ss << vdinfo.name << "_copy_" << i;
      string newname = ss.str();
      vdinfo.id->v(ASTString(newname));
      vdinfo.renames.push_back(newname);
    }

    Model* m_copy = copy(e->envi(), m);
    models.push_back(m_copy);
  }

  vector<Expression*> objs;
  Model* model_0 = models[0];

  SolveI* s0 = model_0->solveItem();
  bool isSat = s0->st() == SolveI::ST_SAT;
  bool hasOut = model_0->outputItem() != nullptr;

  if(!isSat) {
    objs.push_back(s0->e());
  }

  // Remove ann: output items;
  // TODO: figure out what they are
  for(VarDeclI& vdi : model_0->vardecls()) {
    if(vdi.e()->id()->str() == "output")
      vdi.remove();
  }

  for(unsigned int i=1; i<k_models; i++) {
    for(VarDeclI& vdi : models[i]->vardecls()) {
      if(vdi.e()->id()->str() != "output")
        model_0->addItem(&vdi);
    }

    for(ConstraintI& ci : models[i]->constraints()) {
      model_0->addItem(&ci);
    }

    if(hasOut) {
      model_0->addItem(models[i]->outputItem());
    }

    SolveI* si = models[i]->solveItem();
    if(!isSat) {
      objs.push_back(si->e());
    }

    s0->ann().merge(si->ann());
  }

  if(!isSat) {
    vector<Expression*> args;
    args.push_back(new ArrayLit(Location().introduce(), objs));
    s0->e(Call::a(Location().introduce(),  "sum", args));
  }

  e->model(model_0);

  return e;
}

};  // namespace MznTool
