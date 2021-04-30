cmake_minimum_required (VERSION 3.0.0)
project (data_annotation CXX C)

message("-- Configuring: C++ options")
include(cmake-helpers/configure_cpp_14.cmake)

message("-- Configuring: libminzinc")
include(cmake-helpers/configure_libminizinc.cmake)

add_executable(mzn_tool mzn_tool.cpp
  lib/string_utils.cpp
  lib/tool_pass.cpp
  lib/pass_annotate_data_deps.cpp
  lib/pass_inline_includes.cpp
  lib/pass_remove_includes.cpp
  lib/pass_filter_items.cpp
  lib/pass_get_items.cpp
  lib/pass_remove_annotations.cpp
  lib/pass_get_term_types.cpp
  lib/pass_get_data_deps.cpp
  lib/pass_read_model.cpp
  lib/pass_write_model.cpp
  lib/pass_get_exprs.cpp

  include/string_utils.hh
  include/tool_pass.hh
  include/pass_annotate_data_deps.hh
  include/pass_inline_includes.hh
  include/pass_remove_includes.hh
  include/pass_filter_items.hh
  include/pass_get_items.hh
  include/pass_remove_annotations.hh
  include/pass_get_term_types.hh
  include/pass_get_data_deps.hh
  include/pass_read_model.hh
  include/pass_write_model.hh
  include/pass_get_exprs.hh
  )

target_include_directories(mzn_tool PRIVATE include)
target_link_libraries(mzn_tool mzn ${CMAKE_THREAD_LIBS_INIT})

include(cmake-helpers/format.cmake)
