cmake_minimum_required (VERSION 3.0.0)
project (data_annotation CXX C)

message("-- Configuring: C++ options")
include(cmake-helpers/configure_cpp_11.cmake)

message("-- Configuring: libminzinc")
include(cmake-helpers/configure_libminizinc.cmake)

add_executable(data_annotate data_annotate.cpp)
target_link_libraries(data_annotate mzn ${CMAKE_THREAD_LIBS_INIT})

add_executable(data_extract data_extract.cpp)
target_link_libraries(data_extract mzn ${CMAKE_THREAD_LIBS_INIT})


