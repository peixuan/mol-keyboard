# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED REPORT_PATH OR NOT EXISTS "${REPORT_PATH}")
  message(FATAL_ERROR "Latency report is missing: ${REPORT_PATH}")
endif()

file(READ "${REPORT_PATH}" _mol_report)
string(JSON _mol_schema GET "${_mol_report}" schema_version)
string(JSON _mol_method GET "${_mol_report}" method)
string(JSON _mol_events GET "${_mol_report}" event_count)
string(JSON _mol_unmatched GET "${_mol_report}" unmatched_triggers)
string(JSON _mol_p50 GET "${_mol_report}" p50_ms)
string(JSON _mol_p95 GET "${_mol_report}" p95_ms)
string(JSON _mol_max GET "${_mol_report}" maximum_ms)
string(JSON _mol_result GET "${_mol_report}" result)
string(JSON _mol_measurements LENGTH "${_mol_report}" measurements_ms)
string(JSON _mol_hash GET "${_mol_report}" capture_sha256)
string(LENGTH "${_mol_hash}" _mol_hash_length)

if(NOT _mol_schema EQUAL 1 OR NOT _mol_method STREQUAL "dual-channel threshold crossing" OR
   NOT _mol_events EQUAL 20 OR NOT _mol_measurements EQUAL 20 OR
   NOT _mol_unmatched EQUAL 0 OR NOT _mol_result STREQUAL "pass")
  message(FATAL_ERROR "Latency report metadata is invalid: ${_mol_report}")
endif()
if(NOT _mol_p50 EQUAL 19.5 OR NOT _mol_p95 EQUAL 28.05 OR NOT _mol_max EQUAL 29)
  message(FATAL_ERROR
          "Latency percentiles changed: P50=${_mol_p50}, P95=${_mol_p95}, max=${_mol_max}")
endif()
if(NOT _mol_hash MATCHES "^[0-9a-f][0-9a-f]+$" OR NOT _mol_hash_length EQUAL 64)
  message(FATAL_ERROR "Latency capture hash is invalid: ${_mol_hash}")
endif()

message(STATUS "Verified latency fixture report: P50=${_mol_p50} ms, P95=${_mol_p95} ms")
