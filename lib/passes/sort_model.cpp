#include "passes/sort_model.hh"

#include <vector>

using namespace MiniZinc;
using std::string;

namespace MznAnalyse {

SortModel::SortModel() {}

Env* SortModel::run(Env* e, std::ostream& log) {
  Model* m = e->model();

  Model* c = new Model;
  std::vector<Item::ItemId> iids = {
      Item::II_INC, Item::II_FUN, Item::II_VD,  Item::II_CON,
      Item::II_ASN, Item::II_SOL, Item::II_OUT,
  };

  for (Item::ItemId iid : iids) {
    for (Item* item : *m) {
      if (item->iid() == iid) {
        c->addItem(item);
      }
    }
  }

  e->model(c);
  return e;
}
};  // namespace MznAnalyse
