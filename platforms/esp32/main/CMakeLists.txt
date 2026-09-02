# SPDX-License-Identifier: Apache-2.0

set(MOL_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
file(READ "${MOL_REPOSITORY_ROOT}/examples/sequences/scale-study.molseq"
     MOL_SEQUENCE_FIXTURE_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," MOL_SEQUENCE_FIXTURE_BYTES
                     "${MOL_SEQUENCE_FIXTURE_HEX}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
  "${MOL_REPOSITORY_ROOT}/tests/fixtures/sequence_fixture_bytes.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/mol_sequence_fixture_bytes.h" @ONLY)

set(MOL_USB_HID_SOURCES)
set(MOL_USB_HID_REQUIRES)
set(MOL_A2DP_SOURCES)
if(CONFIG_IDF_TARGET_ESP32S3)
  list(APPEND MOL_USB_HID_SOURCES "usb_hid.c")
  list(APPEND MOL_USB_HID_REQUIRES espressif__usb espressif__usb_host_hid)
endif()
if(CONFIG_MOL_A2DP_ENABLE)
  list(APPEND MOL_A2DP_SOURCES "a2dp_source.c")
endif()

idf_component_register(
  SRCS "main.c" "matrix_logic.c" "input_queue.c" "gpio_matrix.c" "device_settings.c"
       "device_storage.c" "device_control.c" "sequence_store.c" "sequence_storage.c" "hid_keyboard.c"
       "bluetooth_hid.c" ${MOL_USB_HID_SOURCES} ${MOL_A2DP_SOURCES}
       "${MOL_REPOSITORY_ROOT}/tests/conformance/sequence_fixture.c"
  PRIV_INCLUDE_DIRS "${MOL_REPOSITORY_ROOT}/tests/conformance"
                    "${CMAKE_CURRENT_BINARY_DIR}/generated"
  REQUIRES mol_core bt esp_hid esp_driver_gpio esp_driver_i2s esp_system esp_timer fatfs nvs_flash
           wear_levelling ${MOL_USB_HID_REQUIRES})
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
