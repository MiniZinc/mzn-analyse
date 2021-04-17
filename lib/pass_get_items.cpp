#include "pass_get_items.hh"

#include <string>
#include <vector>
#include <algorithm>

using namespace MiniZinc;
using std::string;

GetItems::GetItems(const std::vector<MiniZinc::Item::ItemId> &types) {
  const std::vector<Item::ItemId> all_ids = {
    Item::II_INC, Item::II_VD,  Item::II_ASN,
    Item::II_CON, Item::II_SOL, Item::II_OUT,
    Item::II_FUN
  };
  for (MiniZinc::Item::ItemId iid : all_ids) {
    if(std::find(types.begin(), types.end(), iid) == types.end()) {
      item_types.push_back(iid);
    }
  }
}

Env *GetItems::run(Env *e, std::ostream &log) {
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
