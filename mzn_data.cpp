#include <iostream>
#include <vector>
#include <string>

#include "mzn_data.hh"

using namespace MznData;

enum Mode {ANNOTATE, EXTRACT, OBJECTIVE};

using std::string;

int main(int argc, char**argv) {
  Mode mode = ANNOTATE;
  std::vector<string> mzn_paths;
  for(int i=1; i<argc; i++) {
    if(string(argv[i]) == "annotate") {
      mode = ANNOTATE;
    } else if(string(argv[i]) == "extract") {
      mode = EXTRACT;
    } else if(string(argv[i]) == "objective") {
      mode = OBJECTIVE;
    } else {
      mzn_paths.push_back(argv[i]);
    }
  }

  if(mode == ANNOTATE) {
    annotate(mzn_paths);
  } else if(mode == EXTRACT) {
    extract(mzn_paths);
  } else if(mode == OBJECTIVE) {
    objective(mzn_paths);
  }

  return EXIT_SUCCESS;
}
