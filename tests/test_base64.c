/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_internal.h"
#include "sn_test.h"

static void base64(void) {
  const char* yes[] = {"",     "YQ==",  "YWI=", "YWJj",
                       "YR==", "YQ===", "====", "YWJj="};
  const char* no[] = {"YQ", "YQ=", "Y Q==", "YQ==a", "\tYQ=="};
  sn_arena arena = {0};
  sn_bytes decoded;
  for (size_t i = 0; i < sizeof yes / sizeof *yes; i++)
    SN_CHECK(
        sn_base64(&arena, (sn_bytes){(const uint8_t*)yes[i], strlen(yes[i])},
                  false, &decoded),
        "valid base64 %zu", i);
  for (size_t i = 0; i < sizeof no / sizeof *no; i++)
    SN_CHECK(
        !sn_base64(&arena, (sn_bytes){(const uint8_t*)no[i], strlen(no[i])},
                   false, &decoded),
        "invalid base64 %zu", i);
  const uint8_t stored[] = {'Y', ' ', 0, 'Q', '\r', '\n', '=', '='};
  SN_CHECK(
      sn_base64(&arena, (sn_bytes){stored, sizeof stored}, true, &decoded) &&
          decoded.size == 1 && decoded.data[0] == 'a',
      "stored padding");
  sn_arena_free(&arena);
}

int main(void) {
  base64();
  return sn_test_report("base64");
}
