cmake_minimum_required (VERSION 3.5.0)
project (mzn-analyse CXX C)

if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif(POLICY CMP0074)

message("-- Configuring: C++ options")
include(cmake-helpers/configure_cpp_14.cmake)

message("-- Configuring: libminzinc")
include(cmake-helpers/configure_libminizinc.cmake)

add_executable(mzn-analyse mzn-analyse.cpp
  lib/string_utils.cpp
  lib/location_utils.cpp
  lib/tool_pass.cpp
  lib/passes/annotate_data_deps.cpp
  lib/passes/inline_includes.cpp
  lib/passes/remove_includes.cpp
  lib/passes/remove_litter.cpp
  lib/passes/output_all.cpp
  lib/passes/filter_items.cpp
  lib/passes/get_items.cpp
  lib/passes/remove_annotations.cpp
  lib/passes/get_term_types.cpp
  lib/passes/get_solve_anns.cpp
  lib/passes/get_data_deps.cpp
  lib/passes/get_diversity_annotations.cpp
  lib/passes/read_model.cpp
  lib/passes/repeat_model.cpp
  lib/passes/write_model.cpp
  lib/passes/sort_model.cpp
  lib/passes/get_ast.cpp
  lib/passes/get_exprs.cpp
  lib/passes/let_substituter.cpp
  lib/passes/json_tool.cpp
  lib/passes/discretise.cpp
  lib/passes/slackify.cpp

  include/string_utils.hh
  include/location_utils.hh
  include/tool_pass.hh
  include/passes/annotate_data_deps.hh
  include/passes/inline_includes.hh
  include/passes/remove_includes.hh
  include/passes/remove_litter.hh
  include/passes/output_all.hh
  include/passes/filter_items.hh
  include/passes/get_items.hh
  include/passes/remove_annotations.hh
  include/passes/get_term_types.hh
  include/passes/get_solve_anns.hh
  include/passes/get_diversity_annotations.hh
  include/passes/read_model.hh
  include/passes/repeat_model.hh
  include/passes/get_ast.hh
  include/passes/get_exprs.hh
  include/passes/let_substituter.hh
  include/passes/json_tool.hh
  include/passes/discretise.hh
  include/passes/slackify.hh
  include/passes/sort_model.hh
  include/passes/write_model.hh
  )

target_include_directories(mzn-analyse PRIVATE include)
target_link_libraries(mzn-analyse mzn ${CMAKE_THREAD_LIBS_INIT})

include(cmake-helpers/format.cmake)

install(TARGETS mzn-analyse
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
