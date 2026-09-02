# SPDX-License-Identifier: Apache-2.0

if(NOT EXISTS "${FIRST}" OR NOT EXISTS "${SECOND}")
  message(FATAL_ERROR "Sequence round-trip outputs are missing")
endif()
file(SHA256 "${FIRST}" first_hash)
file(SHA256 "${SECOND}" second_hash)
if(NOT first_hash STREQUAL second_hash)
  message(FATAL_ERROR "Sequence round trip changed bytes: ${first_hash} vs ${second_hash}")
endif()
message(STATUS "Mol Sequence JSON round trip is byte-identical: ${first_hash}")
