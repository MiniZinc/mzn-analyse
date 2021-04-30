#pragma once

#include "tool_pass.hh"

#include <string>
#include <vector>

struct ShortLoc
{
    size_t sl; // start line
    size_t sc; // start column
    size_t el; // end line
    size_t ec; // end column
    std::string model_path;
    ShortLoc() = default;
    ShortLoc(const std::string& full_path_entry);
    ShortLoc(const MiniZinc::Location& mzn_loc);

    std::string to_string() const;
    bool contains(const ShortLoc& other) const;
};

class GetExprs : public MiniZinc::Pass {
private:
  std::vector<ShortLoc> locs;

public:
  GetExprs(const std::vector<std::string>& paths);

  MiniZinc::Env *run(MiniZinc::Env *e, std::ostream &log) override;
};
