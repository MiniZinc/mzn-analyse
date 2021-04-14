cmake_minimum_required (VERSION 3.0.0)
project (data_annotation CXX C)

message("-- Configuring: C++ options")
include(cmake-helpers/configure_cpp_11.cmake)

message("-- Configuring: libminzinc")
include(cmake-helpers/configure_libminizinc.cmake)

add_executable(mzn_data mzn_data.cpp data_annotate.cpp data_extract.cpp data_objective.cpp)
target_link_libraries(mzn_data mzn ${CMAKE_THREAD_LIBS_INIT})

include(format.cmake)
