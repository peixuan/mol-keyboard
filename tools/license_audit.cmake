# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_manifest "${MOL_SOURCE_DIR}/third_party/manifest.lock.json")
set(_license "${MOL_SOURCE_DIR}/third_party/licenses/miniaudio-LICENSE.txt")
set(_sbom "${MOL_SOURCE_DIR}/sbom/mol-keyboard.spdx.json")
foreach(_required_file "${_manifest}" "${_license}" "${_sbom}")
  if(NOT EXISTS "${_required_file}")
    message(FATAL_ERROR "Missing dependency audit file: ${_required_file}")
  endif()
endforeach()

file(READ "${_manifest}" _manifest_json)
string(JSON _schema GET "${_manifest_json}" schema_version)
string(JSON _component_count LENGTH "${_manifest_json}" components)
string(JSON _dependency_count LENGTH "${_manifest_json}" dependencies)
string(JSON _ci_name GET "${_manifest_json}" components 0 name)
string(JSON _ci_revision GET "${_manifest_json}" components 0 revision)
string(JSON _emsdk_name GET "${_manifest_json}" components 1 name)
string(JSON _emsdk_revision GET "${_manifest_json}" components 1 revision)
string(JSON _esp_idf_name GET "${_manifest_json}" components 2 name)
string(JSON _esp_idf_revision GET "${_manifest_json}" components 2 revision)
string(JSON _name GET "${_manifest_json}" dependencies 0 name)
string(JSON _version GET "${_manifest_json}" dependencies 0 version)
string(JSON _commit GET "${_manifest_json}" dependencies 0 commit)
string(JSON _archive_hash GET "${_manifest_json}" dependencies 0 archive_sha256)
string(JSON _license_hash GET "${_manifest_json}" dependencies 0 license_file_sha256)
string(JSON _patch_count LENGTH "${_manifest_json}" dependencies 0 local_modifications)

if(NOT _schema EQUAL 1 OR NOT _component_count EQUAL 3 OR
   NOT _dependency_count EQUAL 1 OR NOT _ci_name STREQUAL "actions/checkout" OR
   NOT _ci_revision STREQUAL "de0fac2e4500dabe0009e67214ff5f5447ce83dd" OR
   NOT _emsdk_name STREQUAL "Emscripten SDK" OR
   NOT _emsdk_revision STREQUAL
       "dfb9d1a46c3bb8f52e1e6324be23123b9d73c190" OR
   NOT _esp_idf_name STREQUAL "ESP-IDF" OR
   NOT _esp_idf_revision STREQUAL
       "fff9895c82d744c7237be8847347bdd1b07c6643" OR
   NOT _name STREQUAL "miniaudio" OR NOT _version STREQUAL "0.11.25" OR
   NOT _commit STREQUAL "9634bedb5b5a2ca38c1ee7108a9358a4e233f14d" OR
   NOT _archive_hash STREQUAL
       "1a3a79b80fc6f0b0cc155e28b954a598e0ddfa2db64e2afa8466be88c476fa55" OR
   NOT _patch_count EQUAL 0)
  message(FATAL_ERROR "The miniaudio lock record is incomplete or unexpected")
endif()

file(SHA256 "${_license}" _actual_license_hash)
if(NOT _actual_license_hash STREQUAL _license_hash)
  message(FATAL_ERROR "The miniaudio license snapshot hash does not match the lock")
endif()

file(READ "${_sbom}" _sbom_json)
string(JSON _spdx_version GET "${_sbom_json}" spdxVersion)
string(JSON _package_count LENGTH "${_sbom_json}" packages)
string(JSON _dependency_name GET "${_sbom_json}" packages 1 name)
if(NOT _spdx_version STREQUAL "SPDX-2.3" OR NOT _package_count EQUAL 5 OR
   NOT _dependency_name STREQUAL "miniaudio")
  message(FATAL_ERROR "The SPDX SBOM is incomplete")
endif()

message(STATUS "Dependency license audit passed for miniaudio 0.11.25")
