/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_internal.h"
#include "sn_test.h"

static void rle(void) {
  uint8_t out[20000];
  const uint8_t short_run[] = {0x61, 2, 0x62, 0};
  SN_CHECK(
      sn_rle((sn_bytes){short_run, 4}, 4, 0x4000, out) == SN_RLE_COMPLETE &&
          out[2] == 0x61 && out[3] == 0x62,
      "short runs");
  const uint8_t held[] = {0x61, 0x80, 0x61, 2};
  SN_CHECK(sn_rle((sn_bytes){held, 4}, 131, 0x4000, out) == SN_RLE_COMPLETE,
           "held combines");
  for (size_t index = 0; index < 131; index++)
    SN_CHECK(out[index] == 0x61, "held pixel %zu", index);
  const uint8_t split[] = {0x61, 0x80, 0x62, 2};
  SN_CHECK(sn_rle((sn_bytes){split, 4}, 131, 0x4000, out) == SN_RLE_COMPLETE &&
               out[128] == 0x62,
           "held stands alone");
  const uint8_t tail[] = {0x61, 0x81};
  SN_CHECK(sn_rle((sn_bytes){tail, 2}, 64, 0x4000, out) == SN_RLE_COMPLETE,
           "tail cut");
  const uint8_t block[] = {0x61, 0xff};
  sn_bitmap bitmap = {.bytes = {block, 2}};
  uint8_t* codes = nullptr;
  size_t code_count;
  SN_CHECK(
      sn_bitmap_codes(&bitmap, 1024, 1, &codes, &code_count, nullptr) == SN_OK,
      "blank fallback");
  SN_CHECK(!sn_fits((sn_bytes){block, 2}, 1024),
           "sheet inference never uses blank fallback");
  sn_codes_free(codes);
  codes = nullptr;
  SN_CHECK(
      sn_bitmap_codes(&bitmap, 16384, 1, &codes, &code_count, nullptr) == SN_OK,
      "ordinary block");
  sn_codes_free(codes);
  SN_CHECK(sn_bitmap_codes(&bitmap, SIZE_MAX, 2, &codes, &code_count,
                           nullptr) == SN_ERR_LIMIT,
           "bounded allocation");
  SN_CHECK(sn_rle((sn_bytes){short_run, 4}, 3, 0x4000, out) == SN_RLE_OVERSHOOT,
           "overshoot");
  bitmap.encoding = SN_ENCODING_PNG;
  SN_CHECK(sn_bitmap_codes(&bitmap, 1, 1, &codes, &code_count, nullptr) ==
               SN_ERR_IMAGE_ENCODING,
           "PNG not decoded");
}

int main(void) {
  rle();
  return sn_test_report("rle");
}
