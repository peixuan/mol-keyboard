# SPDX-License-Identifier: Apache-2.0

set(MOL_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")

idf_component_register(
  SRCS "${MOL_REPOSITORY_ROOT}/src/c_api/mol_engine.c"
  INCLUDE_DIRS "${MOL_REPOSITORY_ROOT}/include")

target_compile_definitions(${COMPONENT_LIB} PUBLIC MOL_BUILD_PROFILE=1 MOL_PROFILE_MAX_VOICES=12)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wpedantic -Werror)
