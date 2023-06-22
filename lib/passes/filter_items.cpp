#include "passes/filter_items.hh"

#include <algorithm>
#include <string>
#include <vector>

using namespace MiniZinc;
using MiniZinc::Type;
using std::string;

namespace MznAnalyse {

FilterItems::FilterItems(const std::vector<MiniZinc::Item::ItemId>& types, bool omit,
                         FilterItems::FilterTypeInst ti)
    : item_types{types}, exclude{omit}, typeinst{ti} {}

bool FilterItems::should_remove(Item* item) {
  MiniZinc::Type::Inst mti =
      (typeinst == FilterItems::VAR ? MiniZinc::Type::TI_VAR : MiniZinc::Type::TI_PAR);
  for (MiniZinc::Item::ItemId iid : item_types) {
    if (item->iid() == iid) {
      if (typeinst == FilterItems::ALL) {
        return exclude;
      }
      if (VarDeclI* vdi = item->dynamicCast<VarDeclI>()) {
        if (vdi->e()->type().ti() == mti) {
          return exclude;
        }
      } else if (ConstraintI* ci = item->dynamicCast<ConstraintI>()) {
        if (Expression::type(ci->e()).ti() == mti) {
          return exclude;
        }
      } else if (AssignI* ai = item->dynamicCast<AssignI>()) {
        if (Expression::type(ai->decl()).ti() == mti && Expression::type(ai->e()).ti() == mti) {
          return exclude;
        }
      }
    }
  }
  return !exclude;
}

Env* FilterItems::run(Env* e, std::ostream& log) {
  Model* model = e->model();

  for (size_t i = 0; i < model->size(); i++) {
    Item* item = model->operator[](i);
    if (should_remove(item)) {
      item->remove();
    }
  }
  model->compact();
  return e;
}
};  // namespace MznAnalyse
