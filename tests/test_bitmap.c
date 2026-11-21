/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_composition.h"
#include "sn_internal.h"
#include "sn_test.h"

static void raster(void) {
  const uint8_t above_gray[] = {0, 100, 90}, above_alpha[] = {128, 255, 0},
                below_gray[] = {255, 0, 33}, below_alpha[] = {255, 0, 100};
  sn_raster above = {3, 1, above_gray, above_alpha},
            below = {3, 1, below_gray, below_alpha};
  sn_raster* out = nullptr;
  SN_CHECK(sn_raster_over(&above, &below, &out, nullptr) == SN_OK, "compose");
  SN_CHECK(out && out->gray[0] == 127 && out->gray[1] == 100 &&
               out->gray[2] == 33 && out->alpha[2] == 100,
           "source over");
  sn_raster_free(out);
  out = nullptr;
  SN_CHECK(sn_raster_flatten(&above, 255, &out, nullptr) == SN_OK, "flatten");
  SN_CHECK(
      out && out->gray[0] == 127 && out->gray[2] == 255 && out->alpha[0] == 255,
      "paper");
  sn_raster_free(out);
}

/* A bitmap decode owns its planes independently and applies both channels of
 * a caller palette, including transparency. */
static void bitmap_palette_and_failure_ownership(void) {
  const uint8_t encoded[] = {0x61, 0, 0x62, 0};
  sn_bitmap bitmap = {.encoding = SN_ENCODING_RATTA_RLE,
                      .bytes = {encoded, sizeof encoded}};
  sn_palette palette = sn_palette_current();
  palette.inks[0x61] = (sn_ink){42, 128};
  sn_raster* result = nullptr;
  sn_error error;
  sn_status status = sn_bitmap_raster(&bitmap, 2, 1, &palette, &result, &error);
  SN_CHECK(status == SN_OK && result && result->gray[0] == 42 &&
               result->alpha[0] == 128 && result->alpha[1] == 0,
           "custom palette maps gray and alpha independently");
  sn_raster_free(result);
  sn_raster sentinel = {0};
  result = &sentinel;
  status = sn_bitmap_raster(&bitmap, 3, 1, &palette, &result, &error);
  SN_CHECK(
      status == SN_ERR_MALFORMED && result == &sentinel &&
          error.status == status && strstr(error.message, "RATTA_RLE"),
      "coverage failure preserves caller output and identifies the bitmap");
}

int main(void) {
  raster();
  bitmap_palette_and_failure_ownership();
  return sn_test_report("bitmap");
}
