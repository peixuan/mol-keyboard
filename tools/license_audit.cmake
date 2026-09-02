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
set(_playwright_license
    "${MOL_SOURCE_DIR}/third_party/licenses/playwright-LICENSE.txt")
set(_web_lock "${MOL_SOURCE_DIR}/apps/web/package-lock.json")
set(_apache_license "${MOL_SOURCE_DIR}/LICENSE")
set(_android_build "${MOL_SOURCE_DIR}/platforms/android/build.gradle.kts")
set(_android_wrapper
    "${MOL_SOURCE_DIR}/platforms/android/gradle/wrapper/gradle-wrapper.jar")
set(_android_wrapper_properties
    "${MOL_SOURCE_DIR}/platforms/android/gradle/wrapper/gradle-wrapper.properties")
set(_sbom "${MOL_SOURCE_DIR}/sbom/mol-keyboard.spdx.json")
foreach(_required_file "${_manifest}" "${_miniaudio_license}"
                       "${_oboe_license}" "${_typescript_license}"
                       "${_typescript_notice}" "${_vite_license}"
                       "${_playwright_license}"
                       "${_web_lock}" "${_apache_license}"
                       "${_android_build}" "${_android_wrapper}"
                       "${_android_wrapper_properties}" "${_sbom}")
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
string(JSON _playwright_name GET "${_manifest_json}" components 5 name)
string(JSON _playwright_version GET "${_manifest_json}" components 5 version)
string(JSON _playwright_license_hash GET "${_manifest_json}" components 5
       license_file_sha256)
string(JSON _gradle_name GET "${_manifest_json}" components 6 name)
string(JSON _gradle_version GET "${_manifest_json}" components 6 version)
string(JSON _gradle_archive_hash GET "${_manifest_json}" components 6
       archive_sha256)
string(JSON _gradle_wrapper_hash GET "${_manifest_json}" components 6
       wrapper_jar_sha256)
string(JSON _agp_name GET "${_manifest_json}" components 7 name)
string(JSON _agp_version GET "${_manifest_json}" components 7 version)
string(JSON _agp_artifact_hash GET "${_manifest_json}" components 7
       artifact_sha256)
string(JSON _kotlin_name GET "${_manifest_json}" components 8 name)
string(JSON _kotlin_version GET "${_manifest_json}" components 8 version)
string(JSON _kotlin_artifact_hash GET "${_manifest_json}" components 8
       artifact_sha256)
string(JSON _annotations_name GET "${_manifest_json}" components 9 name)
string(JSON _annotations_version GET "${_manifest_json}" components 9 version)
string(JSON _annotations_artifact_hash GET "${_manifest_json}" components 9
       artifact_sha256)
foreach(_android_component_index RANGE 6 9)
  string(JSON _android_license_hash_${_android_component_index} GET
         "${_manifest_json}" components ${_android_component_index}
         license_file_sha256)
endforeach()
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

if(NOT _schema EQUAL 1 OR NOT _component_count EQUAL 10 OR
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
   NOT _playwright_name STREQUAL "Playwright" OR
   NOT _playwright_version STREQUAL "1.62.1" OR
   NOT _gradle_name STREQUAL "Gradle" OR
   NOT _gradle_version STREQUAL "8.11.1" OR
   NOT _gradle_archive_hash STREQUAL
       "f397b287023acdba1e9f6fc5ea72d22dd63669d59ed4a289a29b1a76eee151c6" OR
   NOT _gradle_wrapper_hash STREQUAL
       "2db75c40782f5e8ba1fc278a5574bab070adccb2d21ca5a6e5ed840888448046" OR
   NOT _agp_name STREQUAL "Android Gradle Plugin" OR
   NOT _agp_version STREQUAL "8.10.1" OR
   NOT _agp_artifact_hash STREQUAL
       "a0fe22ce029c548335a75913f7ad517c827c567b8abb84047102034255ae1173" OR
   NOT _kotlin_name STREQUAL "Kotlin" OR
   NOT _kotlin_version STREQUAL "2.1.20" OR
   NOT _kotlin_artifact_hash STREQUAL
       "1bcc74e8ce84e2c25eaafde10f1248349cce3062b6e36978cbeec610db1e930a" OR
   NOT _annotations_name STREQUAL "JetBrains Java Annotations" OR
   NOT _annotations_version STREQUAL "13.0" OR
   NOT _annotations_artifact_hash STREQUAL
       "ace2a10dc8e2d5fd34925ecac03e4988b2c0f851650c94b8cef49ba1bd111478" OR
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
file(SHA256 "${_playwright_license}" _actual_playwright_license_hash)
if(NOT _actual_playwright_license_hash STREQUAL _playwright_license_hash)
  message(FATAL_ERROR
          "The Playwright license snapshot hash does not match the lock")
endif()
file(SHA256 "${_apache_license}" _actual_apache_license_hash)
foreach(_android_component_index RANGE 6 9)
  if(NOT _actual_apache_license_hash STREQUAL
     _android_license_hash_${_android_component_index})
    message(FATAL_ERROR
            "An Android dependency license hash does not match the lock")
  endif()
endforeach()
file(SHA256 "${_android_wrapper}" _actual_gradle_wrapper_hash)
if(NOT _actual_gradle_wrapper_hash STREQUAL _gradle_wrapper_hash)
  message(FATAL_ERROR "The Gradle wrapper JAR hash does not match the lock")
endif()
file(READ "${_android_wrapper_properties}" _android_wrapper_text)
foreach(_wrapper_record
        "gradle-8.11.1-bin.zip"
        "distributionSha256Sum=f397b287023acdba1e9f6fc5ea72d22dd63669d59ed4a289a29b1a76eee151c6"
        "validateDistributionUrl=true")
  string(FIND "${_android_wrapper_text}" "${_wrapper_record}" _wrapper_position)
  if(_wrapper_position EQUAL -1)
    message(FATAL_ERROR "The Gradle wrapper properties are not checksum locked")
  endif()
endforeach()
file(READ "${_android_build}" _android_build_text)
foreach(_android_plugin_record
        "com.android.application\") version \"8.10.1"
        "org.jetbrains.kotlin.android\") version \"2.1.20")
  string(FIND "${_android_build_text}" "${_android_plugin_record}"
         _android_plugin_position)
  if(_android_plugin_position EQUAL -1)
    message(FATAL_ERROR "The Android build plugins do not match the lock")
  endif()
endforeach()

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
string(JSON _web_playwright_test_version GET "${_web_lock_json}" packages
       "node_modules/@playwright/test" version)
string(JSON _web_playwright_test_license GET "${_web_lock_json}" packages
       "node_modules/@playwright/test" license)
string(JSON _web_playwright_version GET "${_web_lock_json}" packages
       "node_modules/playwright" version)
string(JSON _web_playwright_license GET "${_web_lock_json}" packages
       "node_modules/playwright" license)
string(JSON _web_playwright_core_version GET "${_web_lock_json}" packages
       "node_modules/playwright-core" version)
string(JSON _web_playwright_core_license GET "${_web_lock_json}" packages
       "node_modules/playwright-core" license)
if(NOT _web_lock_version EQUAL 3 OR NOT _web_package_count EQUAL 66 OR
   NOT _web_typescript_version STREQUAL "7.0.2" OR
   NOT _web_typescript_license STREQUAL "Apache-2.0" OR
   NOT _web_vite_version STREQUAL "8.2.2" OR
   NOT _web_vite_license STREQUAL "MIT" OR
   NOT _web_playwright_test_version STREQUAL "1.62.1" OR
   NOT _web_playwright_test_license STREQUAL "Apache-2.0" OR
   NOT _web_playwright_version STREQUAL "1.62.1" OR
   NOT _web_playwright_license STREQUAL "Apache-2.0" OR
   NOT _web_playwright_core_version STREQUAL "1.62.1" OR
   NOT _web_playwright_core_license STREQUAL "Apache-2.0")
  message(FATAL_ERROR "The audited Web package lock is incomplete or unexpected")
endif()

file(READ "${_sbom}" _sbom_json)
string(JSON _spdx_version GET "${_sbom_json}" spdxVersion)
string(JSON _package_count LENGTH "${_sbom_json}" packages)
string(JSON _dependency_name GET "${_sbom_json}" packages 1 name)
string(JSON _oboe_package_name GET "${_sbom_json}" packages 3 name)
string(JSON _typescript_package_name GET "${_sbom_json}" packages 6 name)
string(JSON _vite_package_name GET "${_sbom_json}" packages 7 name)
string(JSON _playwright_package_name GET "${_sbom_json}" packages 8 name)
string(JSON _gradle_package_name GET "${_sbom_json}" packages 9 name)
string(JSON _agp_package_name GET "${_sbom_json}" packages 10 name)
string(JSON _kotlin_package_name GET "${_sbom_json}" packages 11 name)
string(JSON _annotations_package_name GET "${_sbom_json}" packages 12 name)
if(NOT _spdx_version STREQUAL "SPDX-2.3" OR NOT _package_count EQUAL 13 OR
   NOT _dependency_name STREQUAL "miniaudio" OR
   NOT _oboe_package_name STREQUAL "Oboe" OR
   NOT _typescript_package_name STREQUAL "TypeScript" OR
   NOT _vite_package_name STREQUAL "Vite" OR
   NOT _playwright_package_name STREQUAL "Playwright" OR
   NOT _gradle_package_name STREQUAL "Gradle" OR
   NOT _agp_package_name STREQUAL "Android Gradle Plugin" OR
   NOT _kotlin_package_name STREQUAL "Kotlin Standard Library" OR
   NOT _annotations_package_name STREQUAL "JetBrains Java Annotations")
  message(FATAL_ERROR "The SPDX SBOM is incomplete")
endif()

message(STATUS
        "Dependency license audit passed for native, embedded, and Web dependencies")
