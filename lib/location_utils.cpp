#include "location_utils.hh"
#include "string_utils.hh"

#include <minizinc/file_utils.hh>

#include <vector>

using namespace MiniZinc;

ShortLoc::ShortLoc(const std::string& full_path_entry) {
  std::vector<std::string> parts = utils::split(full_path_entry, '|');
  if(parts.size() >= 5) {
    model_path = FileUtils::base_name(parts[0]);
    sl = stoul(parts[1]);
    sc = stoul(parts[2]);
    el = stoul(parts[3]);
    ec = stoul(parts[4]);
  } else {
    std::cerr << "Warning: cannot parse: " << full_path_entry << std::endl;
  }
}

ShortLoc::ShortLoc(const MiniZinc::Location& mzn_loc) {
  const char* mpath = mzn_loc.filename().c_str();
  if(mpath != nullptr) {
    model_path = FileUtils::base_name(std::string(mpath));
  }
  sl = mzn_loc.firstLine();
  sc = mzn_loc.firstColumn();
  el = mzn_loc.lastLine();
  ec = mzn_loc.lastColumn();
}

std::string ShortLoc::to_string() const {
  std::stringstream ss;
  ss << model_path
     << "|" << sl << "|" << sc
     << "|" << el << "|" << ec;
  return ss.str();
}

bool ShortLoc::contains(const ShortLoc& b) const {
  bool cont = model_path == b.model_path
      && (sl < b.sl || (sl == b.sl && sc <= b.sc))
      && (el > b.el || (el == b.el && ec >= b.ec));
  return cont;
}

std::ostream &operator<<(std::ostream &os, const ShortLoc &a) {
  os << a.to_string();
  return os;
}

bool operator==(const ShortLoc& a, const ShortLoc& b) {
  return a.model_path == b.model_path &&
    a.sl == b.sl && a.sc == b.sc &&
    a.el == b.el && a.ec == b.ec;
}
