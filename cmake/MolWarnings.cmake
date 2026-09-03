# SPDX-License-Identifier: Apache-2.0
include_guard(GLOBAL)

function(mol_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8
                                              $<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
      # The OpenHarmony SDK injects --gcc-toolchain for its bundled Clang even
      # though that driver does not consume the option on Windows hosts.
      target_compile_options(${target} PRIVATE -Wno-unused-command-line-argument)
    endif()
  endif()
endfunction()
