cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

# TODO Perf test:
#  - try if setting CRYPTOPP_NATIVE_ARCH=ON and adding -march=native to the compile commands for cryfs source files makes a difference
#    -> if yes, offer a cmake option to enable both of these

project(cryfs)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake-utils)
include(utils)

require_gcc_version(7.0)
require_clang_version(7.0)

# Default value is not to build test cases
option(BUILD_TESTING "build test cases" OFF)
option(CRYFS_UPDATE_CHECKS "let cryfs check for updates and security vulnerabilities" OFF)
option(DISABLE_OPENMP "allow building without OpenMP libraries. This will cause performance degradations." OFF)

# The following options are helpful for development and/or CI
option(USE_WERROR "build with -Werror flag")
option(USE_CLANG_TIDY "build with clang-tidy checks enabled" OFF)
option(USE_IWYU "build with iwyu checks enabled" OFF)
option(CLANG_TIDY_WARNINGS_AS_ERRORS "treat clang-tidy warnings as errors" OFF)

if (MSVC)
    option(DOKAN_PATH "Location of the Dokan library, e.g. C:\\Program Files\\Dokan\\DokanLibrary-2.0.6" "")
endif()

# Default value is to build in release mode but with debug symbols
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE INTERNAL "CMAKE_BUILD_TYPE")
endif(NOT CMAKE_BUILD_TYPE)

# We don't use LTO because crypto++ has problems with it, see https://github.com/weidai11/cryptopp/issues/1031 and https://www.cryptopp.com/wiki/Link_Time_Optimization

# The MSVC version on AppVeyor CI needs this
if(MSVC)
  add_definitions(/bigobj)
endif()

# Hardcoded version. Don't forget to change this when merging upstream!
set(GIT_VERSION "0.11.4-libcryfs")

add_subdirectory(vendor EXCLUDE_FROM_ALL)
add_subdirectory(src)
