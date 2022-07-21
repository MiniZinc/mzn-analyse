#pragma once

#include <minizinc/model.hh>
#include <ostream>
#include <string>

struct ShortLoc {
  size_t sl;  // start line
  size_t sc;  // start column
  size_t el;  // end line
  size_t ec;  // end column
  std::string model_path;
  std::string base_path;
  ShortLoc() = default;
  ShortLoc(const std::string& full_path_entry);
  ShortLoc(const MiniZinc::Location& mzn_loc);

  std::string to_string() const;
  bool contains(const ShortLoc& other) const;
};

std::ostream& operator<<(std::ostream& os, const ShortLoc& a);
bool operator==(const ShortLoc& a, const ShortLoc& b);
