# SPDX-License-Identifier: Apache-2.0

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ARM64)

set(_mol_llvm_mingw_root "$ENV{MOL_LLVM_MINGW_ROOT}")
if(_mol_llvm_mingw_root STREQUAL "")
  message(FATAL_ERROR
          "Set MOL_LLVM_MINGW_ROOT to an extracted LLVM-MinGW UCRT toolchain")
endif()
file(TO_CMAKE_PATH "${_mol_llvm_mingw_root}" _mol_llvm_mingw_root)
set(_mol_llvm_mingw_bin "${_mol_llvm_mingw_root}/bin")

foreach(_mol_tool clang clang++ windres)
  if(NOT EXISTS "${_mol_llvm_mingw_bin}/aarch64-w64-mingw32-${_mol_tool}.exe")
    message(FATAL_ERROR
            "MOL_LLVM_MINGW_ROOT is missing aarch64-w64-mingw32-${_mol_tool}.exe")
  endif()
endforeach()

set(CMAKE_C_COMPILER
    "${_mol_llvm_mingw_bin}/aarch64-w64-mingw32-clang.exe" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER
    "${_mol_llvm_mingw_bin}/aarch64-w64-mingw32-clang++.exe" CACHE FILEPATH "")
set(CMAKE_RC_COMPILER
    "${_mol_llvm_mingw_bin}/aarch64-w64-mingw32-windres.exe" CACHE FILEPATH "")
set(CMAKE_FIND_ROOT_PATH "${_mol_llvm_mingw_root}/aarch64-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
