/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_internal.h"
#include "sn_test.h"

static void json(void) {
  sn_arena arena = {0};
  const char* yes =
      "{\"s\":\"\\ud83d\\ude00\",\"array\":[null,true,false,-1.25e2]}";
  sn_json* root =
      sn_json_read(&arena, (sn_bytes){(const uint8_t*)yes, strlen(yes)});
  SN_CHECK(root && sn_json_get(root, "s")->string.length == 4,
           "surrogate pair");
  const char* no[] = {"[1,,]",        "{\"x\":01}", "{\"x\":1.}", "\"\\ud800\"",
                      "[true false]", "{} garbage", "[NaN]"};
  for (size_t i = 0; i < sizeof no / sizeof *no; i++)
    SN_CHECK(
        !sn_json_read(&arena, (sn_bytes){(const uint8_t*)no[i], strlen(no[i])}),
        "invalid JSON %zu", i);
  const uint8_t wide[] = {'{', 0, '}', 0};
  SN_CHECK(sn_json_read(&arena, (sn_bytes){wide, sizeof wide}) != nullptr,
           "wide Unicode JSON");
  const uint8_t trailing[] = "[1,]";
  SN_CHECK(sn_json_read(&arena, (sn_bytes){trailing, sizeof trailing - 1}) !=
               nullptr,
           "trailing comma");
  uint8_t deep[132];
  memset(deep, '[', 66);
  memset(deep + 66, ']', 66);
  SN_CHECK(!sn_json_read(&arena, (sn_bytes){deep, sizeof deep}),
           "bounded nesting");
  sn_page page = {0};
  const char* table =
      "[{\"layerId\"#1,\"name\"#\"ink#hash\",\"isVisible\"#false}]";
  sn_layer_table(&arena, (sn_string){table, strlen(table)}, &page);
  SN_CHECK(page.layer_info_count == 1 && !page.layer_info[0].is_visible &&
               sn_equal(page.layer_info[0].name, "ink#hash"),
           "escaped layer table");
  page = (sn_page){0};
  table = "[{\"layerId\":\"1\"}]";
  sn_layer_table(&arena, (sn_string){table, strlen(table)}, &page);
  SN_CHECK(!page.layer_info_count, "strict typed field");
  sn_arena_free(&arena);
}

int main(void) {
  json();
  return sn_test_report("json");
}
