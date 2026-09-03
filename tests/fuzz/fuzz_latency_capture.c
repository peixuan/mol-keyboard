/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define main mol_latency_probe_cli_main
#include "../../tools/latency-probe/main.c"
#undef main

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  mol_capture_t capture;
  FILE* file;
  size_t payload_size;
  if (size > 1024u * 1024u) return 0;
  file = tmpfile();
  if (file == NULL) return 0;
  if (size > 0u && (data[0] & 1u) == 0u) {
    payload_size = (size - 1u) & ~(size_t)3u;
    (void)mol_write_exact(file, "RIFF", 4u);
    (void)mol_write_u32_le(file, (uint32_t)(36u + payload_size));
    (void)mol_write_exact(file, "WAVEfmt ", 8u);
    (void)mol_write_u32_le(file, 16u);
    (void)mol_write_u16_le(file, 1u);
    (void)mol_write_u16_le(file, 2u);
    (void)mol_write_u32_le(file, 48000u);
    (void)mol_write_u32_le(file, 192000u);
    (void)mol_write_u16_le(file, 4u);
    (void)mol_write_u16_le(file, 16u);
    (void)mol_write_exact(file, "data", 4u);
    (void)mol_write_u32_le(file, (uint32_t)payload_size);
    (void)fwrite(data + 1u, 1u, payload_size, file);
  } else {
    (void)fwrite(data, 1u, size, file);
  }
  if (fflush(file) == 0 && fseek(file, 0, SEEK_SET) == 0 &&
      mol_load_capture_stream(file, &capture)) {
    if (capture.samples == NULL || capture.channels < 2u || capture.frame_count == 0u) {
      __builtin_trap();
    }
    free(capture.samples);
  }
  (void)fclose(file);
  return 0;
}
