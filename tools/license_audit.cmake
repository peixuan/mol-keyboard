# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_manifest "${MOL_SOURCE_DIR}/third_party/manifest.lock.json")
set(_miniaudio_license
    "${MOL_SOURCE_DIR}/third_party/licenses/miniaudio-LICENSE.txt")
set(_oboe_license "${MOL_SOURCE_DIR}/third_party/licenses/oboe-LICENSE.txt")
set(_typescript_license
    "${MOL_SOURCE_DIR}/third_party/licenses/typescript-LICENSE.txt")
set(_typescript_notice
    "${MOL_SOURCE_DIR}/third_party/licenses/typescript-NOTICE.txt")
set(_vite_license "${MOL_SOURCE_DIR}/third_party/licenses/vite-LICENSE.md")
set(_web_lock "${MOL_SOURCE_DIR}/apps/web/package-lock.json")
set(_sbom "${MOL_SOURCE_DIR}/sbom/mol-keyboard.spdx.json")
foreach(_required_file "${_manifest}" "${_miniaudio_license}"
                       "${_oboe_license}" "${_typescript_license}"
                       "${_typescript_notice}" "${_vite_license}"
                       "${_web_lock}" "${_sbom}")
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
string(JSON _typescript_name GET "${_manifest_json}" components 3 name)
string(JSON _typescript_version GET "${_manifest_json}" components 3 version)
string(JSON _typescript_license_hash GET "${_manifest_json}" components 3
       license_file_sha256)
string(JSON _typescript_notice_hash GET "${_manifest_json}" components 3
       notice_file_sha256)
string(JSON _vite_name GET "${_manifest_json}" components 4 name)
string(JSON _vite_version GET "${_manifest_json}" components 4 version)
string(JSON _vite_revision GET "${_manifest_json}" components 4 revision)
string(JSON _vite_license_hash GET "${_manifest_json}" components 4
       license_file_sha256)
string(JSON _name GET "${_manifest_json}" dependencies 0 name)
string(JSON _version GET "${_manifest_json}" dependencies 0 version)
string(JSON _commit GET "${_manifest_json}" dependencies 0 commit)
string(JSON _archive_hash GET "${_manifest_json}" dependencies 0 archive_sha256)
string(JSON _license_hash GET "${_manifest_json}" dependencies 0 license_file_sha256)
string(JSON _patch_count LENGTH "${_manifest_json}" dependencies 0 local_modifications)
string(JSON _oboe_name GET "${_manifest_json}" dependencies 1 name)
string(JSON _oboe_version GET "${_manifest_json}" dependencies 1 version)
string(JSON _oboe_commit GET "${_manifest_json}" dependencies 1 commit)
string(JSON _oboe_archive_hash GET "${_manifest_json}" dependencies 1 archive_sha256)
string(JSON _oboe_license_hash GET "${_manifest_json}" dependencies 1 license_file_sha256)
string(JSON _oboe_patch_count LENGTH "${_manifest_json}" dependencies 1 local_modifications)

if(NOT _schema EQUAL 1 OR NOT _component_count EQUAL 5 OR
   NOT _dependency_count EQUAL 2 OR NOT _ci_name STREQUAL "actions/checkout" OR
   NOT _ci_revision STREQUAL "de0fac2e4500dabe0009e67214ff5f5447ce83dd" OR
   NOT _emsdk_name STREQUAL "Emscripten SDK" OR
   NOT _emsdk_revision STREQUAL
       "dfb9d1a46c3bb8f52e1e6324be23123b9d73c190" OR
   NOT _esp_idf_name STREQUAL "ESP-IDF" OR
   NOT _esp_idf_revision STREQUAL
       "fff9895c82d744c7237be8847347bdd1b07c6643" OR
   NOT _typescript_name STREQUAL "TypeScript" OR
   NOT _typescript_version STREQUAL "7.0.2" OR
   NOT _vite_name STREQUAL "Vite" OR NOT _vite_version STREQUAL "8.2.2" OR
   NOT _vite_revision STREQUAL "2bd066d87f5bafd315be9f40889d0a60b9e58e0b" OR
   NOT _name STREQUAL "miniaudio" OR NOT _version STREQUAL "0.11.25" OR
   NOT _commit STREQUAL "9634bedb5b5a2ca38c1ee7108a9358a4e233f14d" OR
   NOT _archive_hash STREQUAL
       "1a3a79b80fc6f0b0cc155e28b954a598e0ddfa2db64e2afa8466be88c476fa55" OR
   NOT _patch_count EQUAL 0 OR NOT _oboe_name STREQUAL "Oboe" OR
   NOT _oboe_version STREQUAL "1.10.0" OR
   NOT _oboe_commit STREQUAL "a81bb9f87d4105b84b682685d3bfbb5beca371d1" OR
   NOT _oboe_archive_hash STREQUAL
       "0e4245f8860c4287040a5d76501c588490bcc9cb57614c486c0c201a5dde3e9f" OR
   NOT _oboe_patch_count EQUAL 0)
  message(FATAL_ERROR "The runtime dependency lock records are incomplete or unexpected")
endif()

file(SHA256 "${_miniaudio_license}" _actual_license_hash)
if(NOT _actual_license_hash STREQUAL _license_hash)
  message(FATAL_ERROR "The miniaudio license snapshot hash does not match the lock")
endif()
file(SHA256 "${_oboe_license}" _actual_oboe_license_hash)
if(NOT _actual_oboe_license_hash STREQUAL _oboe_license_hash)
  message(FATAL_ERROR "The Oboe license snapshot hash does not match the lock")
endif()
file(SHA256 "${_typescript_license}" _actual_typescript_license_hash)
if(NOT _actual_typescript_license_hash STREQUAL _typescript_license_hash)
  message(FATAL_ERROR "The TypeScript license snapshot hash does not match the lock")
endif()
file(SHA256 "${_typescript_notice}" _actual_typescript_notice_hash)
if(NOT _actual_typescript_notice_hash STREQUAL _typescript_notice_hash)
  message(FATAL_ERROR "The TypeScript notice snapshot hash does not match the lock")
endif()
file(SHA256 "${_vite_license}" _actual_vite_license_hash)
if(NOT _actual_vite_license_hash STREQUAL _vite_license_hash)
  message(FATAL_ERROR "The Vite license snapshot hash does not match the lock")
endif()

file(READ "${_web_lock}" _web_lock_json)
string(JSON _web_lock_version GET "${_web_lock_json}" lockfileVersion)
string(JSON _web_package_count LENGTH "${_web_lock_json}" packages)
string(JSON _web_typescript_version GET "${_web_lock_json}" packages
       "node_modules/typescript" version)
string(JSON _web_typescript_license GET "${_web_lock_json}" packages
       "node_modules/typescript" license)
string(JSON _web_vite_version GET "${_web_lock_json}" packages
       "node_modules/vite" version)
string(JSON _web_vite_license GET "${_web_lock_json}" packages
       "node_modules/vite" license)
if(NOT _web_lock_version EQUAL 3 OR NOT _web_package_count EQUAL 62 OR
   NOT _web_typescript_version STREQUAL "7.0.2" OR
   NOT _web_typescript_license STREQUAL "Apache-2.0" OR
   NOT _web_vite_version STREQUAL "8.2.2" OR
   NOT _web_vite_license STREQUAL "MIT")
  message(FATAL_ERROR "The audited Web package lock is incomplete or unexpected")
endif()

file(READ "${_sbom}" _sbom_json)
string(JSON _spdx_version GET "${_sbom_json}" spdxVersion)
string(JSON _package_count LENGTH "${_sbom_json}" packages)
string(JSON _dependency_name GET "${_sbom_json}" packages 1 name)
string(JSON _oboe_package_name GET "${_sbom_json}" packages 3 name)
string(JSON _typescript_package_name GET "${_sbom_json}" packages 6 name)
string(JSON _vite_package_name GET "${_sbom_json}" packages 7 name)
if(NOT _spdx_version STREQUAL "SPDX-2.3" OR NOT _package_count EQUAL 8 OR
   NOT _dependency_name STREQUAL "miniaudio" OR
   NOT _oboe_package_name STREQUAL "Oboe" OR
   NOT _typescript_package_name STREQUAL "TypeScript" OR
   NOT _vite_package_name STREQUAL "Vite")
  message(FATAL_ERROR "The SPDX SBOM is incomplete")
endif()

message(STATUS
        "Dependency license audit passed for native, embedded, and Web dependencies")
