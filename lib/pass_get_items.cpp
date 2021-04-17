#include "pass_get_items.hh"

#include <string>
#include <vector>
#include <algorithm>

using namespace MiniZinc;
using std::string;

GetItems::GetItems(const std::vector<size_t> &idxs) {
  indexes.insert(idxs.begin(), idxs.end());
}

Env *GetItems::run(Env *e, std::ostream &log) {
  Model *model = e->model();

  for (size_t i = 0; i < model->size(); i++) {
    Item *item = model->operator[](i);
    if (indexes.find(i) == indexes.end()) {
      item->remove();
    }
  }
  model->compact();
  return e;
}
