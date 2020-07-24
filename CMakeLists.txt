cmake_minimum_required (VERSION 3.0.0)
project (mzn2exps CXX C)

message("-- Configuring: C++ options")
include(cmake-helpers/configure_cpp_11.cmake)

message("-- Configuring: libminzinc")
include(cmake-helpers/configure_libminizinc.cmake)

add_executable(mzn2exps main.cpp)
target_link_libraries(mzn2exps mzn ${CMAKE_THREAD_LIBS_INIT})


