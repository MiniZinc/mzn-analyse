#include "pass_remove_items.hh"

#include <string>

using namespace MiniZinc;
using std::string;

RemoveItems::RemoveItems(const std::vector<MiniZinc::Item::ItemId> &types)
    : item_types{types} {}

Env *RemoveItems::run(Env *e, std::ostream &log) {
  Model *model = e->model();

  for (size_t i = 0; i < model->size(); i++) {
    Item *item = model->operator[](i);
    for (MiniZinc::Item::ItemId iid : item_types) {
      if (item->iid() == iid) {
        item->remove();
        break;
      }
    }
  }
  model->compact();
  return e;
}
