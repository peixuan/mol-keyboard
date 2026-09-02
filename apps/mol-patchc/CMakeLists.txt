# SPDX-License-Identifier: Apache-2.0

add_executable(mol-patchc main.c)
target_link_libraries(mol-patchc PRIVATE mol::core)
set_target_properties(mol-patchc PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol-patchc)
if(MSVC)
  target_compile_definitions(mol-patchc PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

install(TARGETS mol-patchc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
