# SPDX-License-Identifier: Apache-2.0

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  message(FATAL_ERROR "The iOS application target requires an iOS toolchain.")
endif()

set(MOL_APPLE_BUNDLE_IDENTIFIER
    "cn.zhangpeixuan.molkeyboard"
    CACHE STRING "Reverse-DNS bundle identifier for the iOS application")
set(MOL_APPLE_DEVELOPMENT_TEAM
    ""
    CACHE STRING "Optional Apple Development Team identifier")
set(_mol_web_dist "${PROJECT_SOURCE_DIR}/apps/web/dist")
if(NOT EXISTS "${_mol_web_dist}/index.html")
  message(FATAL_ERROR
          "The production Web UI is missing. Run platforms/ios/build-app.sh first.")
endif()

enable_language(OBJCXX)
add_executable(
  mol_ios_app MACOSX_BUNDLE
  main.mm
  MOLAppDelegate.h
  MOLAppDelegate.mm
  MOLNativeAudioController.h
  MOLNativeAudioController.mm
  MOLViewController.h
  MOLViewController.mm
  Assets.xcassets)
target_compile_features(mol_ios_app PRIVATE cxx_std_17)
target_compile_options(mol_ios_app PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_link_libraries(mol_ios_app PRIVATE mol_apple_audio "-framework UIKit"
                                                "-framework WebKit")
set_target_properties(
  mol_ios_app
  PROPERTIES OUTPUT_NAME "MoLKeyboard"
             OBJCXX_STANDARD 17
             OBJCXX_STANDARD_REQUIRED ON
             OBJCXX_EXTENSIONS OFF
             MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
             MACOSX_BUNDLE_GUI_IDENTIFIER "${MOL_APPLE_BUNDLE_IDENTIFIER}"
             MACOSX_BUNDLE_BUNDLE_NAME "MoL Keyboard"
             MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
             MACOSX_BUNDLE_BUNDLE_VERSION "1"
             XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${MOL_APPLE_BUNDLE_IDENTIFIER}"
             XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
             XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
             XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
if(MOL_APPLE_DEVELOPMENT_TEAM)
  set_target_properties(
    mol_ios_app
    PROPERTIES XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${MOL_APPLE_DEVELOPMENT_TEAM}")
endif()
mol_enable_warnings(mol_ios_app)

add_custom_command(
  TARGET mol_ios_app
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory "${_mol_web_dist}"
    "$<TARGET_BUNDLE_CONTENT_DIR:mol_ios_app>/web"
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/PrivacyInfo.xcprivacy"
    "$<TARGET_BUNDLE_CONTENT_DIR:mol_ios_app>/PrivacyInfo.xcprivacy"
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/en.lproj"
    "$<TARGET_BUNDLE_CONTENT_DIR:mol_ios_app>/en.lproj"
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/zh-Hans.lproj"
    "$<TARGET_BUNDLE_CONTENT_DIR:mol_ios_app>/zh-Hans.lproj"
  VERBATIM)

set_source_files_properties(
  Assets.xcassets PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

install(TARGETS mol_ios_app BUNDLE DESTINATION .)
