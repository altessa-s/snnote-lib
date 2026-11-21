/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_internal.h"
#include "sn_test.h"

static void palettes(void) {
  sn_palette old = sn_palette_legacy(), current = sn_palette_current();
  SN_CHECK(old.inks[0].alpha == 0 && old.inks[0x63].gray == 157,
           "legacy codes");
  SN_CHECK(current.inks[0].gray == 97 && current.inks[0xff].gray == 98 &&
               current.inks[0x62].alpha == 0,
           "current swaps");
  SN_CHECK(sn_palette_for_format(20220011).inks[0].alpha == 0,
           "legacy version");
  SN_CHECK(sn_palette_for_format(20230015).inks[0].alpha == 255,
           "current version");
  for (size_t i = 0; i < 256; i++)
    SN_CHECK(current.inks[i].alpha == (i == 0x62 ? 0 : 255), "coverage %zu", i);
}

/* Color IDs are independent of bitmap palette codes and marker pen IDs. */
static void derived_color_and_title_values(void) {
  const uint32_t colors[] = {0,   1,   48,  49,  80,  81, 157,
                             158, 201, 202, 254, 255, 42};
  const uint8_t grays[] = {0,   0,   157, 157, 201, 201, 157,
                           157, 201, 201, 254, 255, 42};
  for (size_t index = 0; index < sizeof colors / sizeof *colors; index++) {
    uint8_t gray = 0;
    SN_CHECK(sn_ink_color_gray(colors[index], &gray) && gray == grays[index],
             "color gray %zu", index);
    bool marker = colors[index] == 1 || colors[index] == 49 ||
                  colors[index] == 81 || colors[index] == 158 ||
                  colors[index] == 202;
    SN_CHECK(sn_ink_color_is_marker(colors[index]) == marker,
             "marker color %zu", index);
  }
  uint8_t gray = 123;
  SN_CHECK(!sn_ink_color_gray(256, &gray) && gray == 123,
           "unknown color leaves output alone");
  SN_CHECK(!sn_ink_color_gray(0, nullptr), "null gray output accepted");
  SN_CHECK(sn_title_style_background(1157254) == 157 &&
               sn_title_style_foreground(1157254) == 254,
           "title decimal components");
  SN_CHECK(
      sn_title_style_is_hatched(1000000) && !sn_title_style_is_hatched(1000254),
      "hatched title predicate");
}

int main(void) {
  palettes();
  derived_color_and_title_values();
  return sn_test_report("palette");
}
