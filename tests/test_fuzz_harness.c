/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_internal.h"
#include "sn_test.h"

static size_t calls, last_width, last_height;
static sn_status count_raster(const sn_bitmap* source, size_t width,
                              size_t height, const sn_palette* palette,
                              sn_raster** out, sn_error* err) {
  (void)source;
  (void)palette;
  (void)err;
  calls++;
  last_width = width;
  last_height = height;
  *out = nullptr;
  return SN_ERR_IMAGE_ENCODING;
}

/* Test the actual fuzz traversal, counting expensive decoder calls without
 * allocating full sheet planes. The sanitized smoke run covers real decoding.
 */
#define sn_bitmap_raster count_raster
#include "fuzz_parse.c"
#undef sn_bitmap_raster

int main(void) {
  sn_bitmap source = {.encoding = SN_ENCODING_RATTA_RLE};
  bitmap(&source, 0);
  SN_CHECK(calls == 1, "one decode for the smaller sheet");
  SN_CHECK(last_width == 1404 && last_height == 1872, "smaller sheet size");
  calls = 0;
  bitmap(&source, 1);
  SN_CHECK(calls == 1, "one decode for the larger sheet");
  SN_CHECK(last_width == 1920 && last_height == 2560, "larger sheet size");
  calls = 0;
  bitmap(nullptr, 0);
  source.encoding = SN_ENCODING_PNG;
  bitmap(&source, 0);
  SN_CHECK(calls == 0, "absent and non-RLE bitmaps do not invoke decoding");
  return sn_test_report("fuzz_harness");
}
