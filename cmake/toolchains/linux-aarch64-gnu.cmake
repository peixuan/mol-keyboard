# SPDX-License-Identifier: Apache-2.0

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(_mol_cross_root "$ENV{MOL_LINUX_AARCH64_ROOT}")
if(_mol_cross_root STREQUAL "")
  set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc CACHE FILEPATH "")
  set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++ CACHE FILEPATH "")
else()
  file(TO_CMAKE_PATH "${_mol_cross_root}" _mol_cross_root)
  set(_mol_cross_bin "${_mol_cross_root}/usr/bin")
  set(_mol_cross_gcc_root "${_mol_cross_root}/usr/lib/gcc-cross/aarch64-linux-gnu")
  file(GLOB _mol_cross_gcc_versions LIST_DIRECTORIES true "${_mol_cross_gcc_root}/*")
  list(LENGTH _mol_cross_gcc_versions _mol_cross_gcc_version_count)
  if(NOT _mol_cross_gcc_version_count EQUAL 1)
    message(FATAL_ERROR
            "MOL_LINUX_AARCH64_ROOT must contain exactly one GCC runtime under "
            "usr/lib/gcc-cross/aarch64-linux-gnu")
  endif()
  list(GET _mol_cross_gcc_versions 0 _mol_cross_gcc_runtime)
  if(NOT EXISTS "${_mol_cross_bin}/aarch64-linux-gnu-gcc" OR
     NOT EXISTS "${_mol_cross_bin}/aarch64-linux-gnu-g++")
    message(FATAL_ERROR
            "MOL_LINUX_AARCH64_ROOT does not contain the AArch64 GNU compilers")
  endif()

  set(CMAKE_C_COMPILER "${_mol_cross_bin}/aarch64-linux-gnu-gcc" CACHE FILEPATH "")
  set(CMAKE_CXX_COMPILER "${_mol_cross_bin}/aarch64-linux-gnu-g++" CACHE FILEPATH "")
  set(CMAKE_SYSROOT "${_mol_cross_root}" CACHE PATH "")
  string(APPEND CMAKE_C_FLAGS_INIT
         " -B${_mol_cross_gcc_runtime}/ -B${_mol_cross_bin}/")
  string(APPEND CMAKE_CXX_FLAGS_INIT
         " -B${_mol_cross_gcc_runtime}/ -B${_mol_cross_bin}/")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
