find_package(libminizinc)

if(NOT libminizinc_FOUND)
  error("cmake-helpers/configure_libminizinc.cmake: Cannot find system libminizinc needed for FindMUS.")
endif()

message("cmake-helpers/configure_libminizinc.cmake: Using includes: ${libminizinc_INCLUDE_DIRS}")
include_directories(${libminizinc_INCLUDE_DIRS})
