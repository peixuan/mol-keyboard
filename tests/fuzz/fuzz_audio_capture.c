/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define main mol_audio_analyze_cli_main
#include "../../tools/audio-analyze/main.c"
#undef main

static void fuzz_write_u16_le(FILE* file, uint16_t value) {
  const uint8_t bytes[2] = {(uint8_t)(value & 0xffu), (uint8_t)(value >> 8u)};
  (void)fwrite(bytes, 1u, sizeof(bytes), file);
}

static void fuzz_write_u32_le(FILE* file, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)(value & 0xffu), (uint8_t)((value >> 8u) & 0xffu),
                            (uint8_t)((value >> 16u) & 0xffu), (uint8_t)(value >> 24u)};
  (void)fwrite(bytes, 1u, sizeof(bytes), file);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  mol_wav_t wav;
  FILE* file;
  size_t payload_size;
  if (size > 1024u * 1024u) return 0;
  file = tmpfile();
  if (file == NULL) return 0;
  if (size > 0u && (data[0] & 1u) == 0u) {
    const uint8_t channels = (data[0] & 2u) != 0u ? 2u : 1u;
    const size_t alignment = (size_t)channels * 2u;
    payload_size = (size - 1u) / alignment * alignment;
    (void)fwrite("RIFF", 1u, 4u, file);
    fuzz_write_u32_le(file, (uint32_t)(36u + payload_size));
    (void)fwrite("WAVEfmt ", 1u, 8u, file);
    fuzz_write_u32_le(file, 16u);
    fuzz_write_u16_le(file, 1u);
    fuzz_write_u16_le(file, channels);
    fuzz_write_u32_le(file, 48000u);
    fuzz_write_u32_le(file, 48000u * (uint32_t)alignment);
    fuzz_write_u16_le(file, (uint16_t)alignment);
    fuzz_write_u16_le(file, 16u);
    (void)fwrite("data", 1u, 4u, file);
    fuzz_write_u32_le(file, (uint32_t)payload_size);
    (void)fwrite(data + 1u, 1u, payload_size, file);
  } else {
    (void)fwrite(data, 1u, size, file);
  }
  if (fflush(file) == 0 && fseek(file, 0, SEEK_SET) == 0 && mol_load_wav_stream(file, &wav)) {
    if (wav.samples == NULL || wav.frame_count < 128u || wav.channels < 1u || wav.channels > 2u) {
      __builtin_trap();
    }
    free(wav.samples);
  }
  (void)fclose(file);
  return 0;
}
