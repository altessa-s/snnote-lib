/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_internal.h"
#include "sn_test.h"

static void classification(void) {
  sn_stroke stroke = {.pen = 10, .role_raw = 5000, .kind_name = {"others", 6}};
  sn_classify(&stroke);
  SN_CHECK(stroke.is_drawn, "ink");
  int32_t roles[] = {-1, -2, -4, -5};
  for (size_t i = 0; i < 4; i++) {
    stroke.role_raw = roles[i];
    sn_classify(&stroke);
    SN_CHECK(!stroke.is_drawn, "tool role");
  }
  stroke.role_raw = 5000;
  stroke.has_style = true;
  uint32_t styles[] = {0, 1000254, 5000000, 20000000, 24000001, 25000000, 999};
  sn_category expected[] = {SN_CATEGORY_ORDINARY,     SN_CATEGORY_HEADING,
                            SN_CATEGORY_TEXT_BOX,     SN_CATEGORY_LINK,
                            SN_CATEGORY_PICTURE_LINK, SN_CATEGORY_TEXT_LINK,
                            SN_CATEGORY_OTHER};
  for (size_t i = 0; i < 7; i++) {
    stroke.style = styles[i];
    sn_classify(&stroke);
    SN_CHECK(stroke.category == expected[i], "style range");
  }
  stroke.style = 0;
  stroke.pen = -3;
  sn_classify(&stroke);
  SN_CHECK(!stroke.is_drawn, "negative non-ink pen is excluded");
  stroke.pen = SN_PEN_NEEDLE_POINT;
  int32_t statuses[] = {-2, -3, -4, -16, -99};
  for (size_t i = 0; i < 5; i++) {
    stroke.status_raw = statuses[i];
    sn_classify(&stroke);
    SN_CHECK(!stroke.is_drawn && stroke.status != SN_STROKE_STATUS_OTHER,
             "removed status");
  }
}

/* Derived identities retain both aliases and the raw model's unknown case. */
static void equipment_layers_and_pen_codes(void) {
  const char* names[] = {"A5X", "A6X", "N6", "A6X2", "N5", "A5X2"};
  for (size_t index = 0; index < sizeof names / sizeof *names; index++) {
    sn_size sheet = sn_equipment_sheet(
        sn_equipment_of((sn_string){names[index], strlen(names[index])}));
    SN_CHECK(sheet.width == (index >= 4 ? 1920 : 1404) &&
                 sheet.height == (index >= 4 ? 2560 : 1872),
             "equipment alias %zu", index);
  }
  SN_CHECK(sn_equipment_sheet(SN_EQUIPMENT_OTHER).width == 0,
           "unknown equipment has no portrait sheet");
  const char* keys[] = {"MAINLAYER", "LAYER1", "LAYER2", "LAYER3", "BGLAYER"};
  for (int index = 0; index < 5; index++) {
    SN_CHECK(strcmp(sn_layer_name_key((sn_layer_name)index), keys[index]) == 0,
             "layer key");
    SN_CHECK(sn_layer_name_identifier((sn_layer_name)index) ==
                 (index == 4 ? -1 : index),
             "layer identifier");
  }
  SN_CHECK(!sn_layer_name_key((sn_layer_name)99) &&
               sn_layer_name_identifier((sn_layer_name)99) == -2,
           "invalid layer enum");
  for (int32_t pen = -4; pen < 20; pen++)
    SN_CHECK(sn_pen_is_marker(pen) == (pen == 5 || pen == 11), "marker pen %d",
             pen);
}

int main(void) {
  classification();
  equipment_layers_and_pen_codes();
  return sn_test_report("classification");
}
