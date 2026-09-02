# SPDX-License-Identifier: Apache-2.0

add_executable(mol_core_wasm wasm_exports.c)
target_link_libraries(mol_core_wasm PRIVATE mol::core)
set_target_properties(
  mol_core_wasm
  PROPERTIES C_EXTENSIONS OFF
             C_STANDARD_REQUIRED ON
             OUTPUT_NAME mol_core
             SUFFIX .mjs)
mol_enable_warnings(mol_core_wasm)
target_link_options(
  mol_core_wasm
  PRIVATE --no-entry
          -sWASM=1
          -sMODULARIZE=1
          -sEXPORT_ES6=1
          -sENVIRONMENT=web,worker,node
          -sALLOW_MEMORY_GROWTH=0
          -sINITIAL_MEMORY=16777216
          "-sEXPORTED_FUNCTIONS=['_mol_wasm_initialize','_mol_wasm_note_on','_mol_wasm_note_off','_mol_wasm_render']"
          "-sEXPORTED_RUNTIME_METHODS=['cwrap','HEAPF32']")

if(MOL_BUILD_TESTS)
  if(DEFINED ENV{EMSDK_NODE})
    set(MOL_NODE_EXECUTABLE "$ENV{EMSDK_NODE}")
  else()
    find_program(MOL_NODE_EXECUTABLE node REQUIRED NO_CMAKE_FIND_ROOT_PATH)
  endif()
  add_test(
    NAME mol_wasm_c4_conformance
    COMMAND ${MOL_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_wasm.mjs
            $<TARGET_FILE:mol_core_wasm>)
endif()
