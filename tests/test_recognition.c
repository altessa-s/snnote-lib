/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_encoded.h"

int main(void) {
  sn_arena arena = {0};
  const sn_recognition* recognition = sn_recognition_read(
      &arena, encoded(&arena,
                      "{\"elements\":[{\"type\":\"Text\",\"label\":\"one\"},{"
                      "\"type\":\"Shape\",\"label\":\"ignored\"},{\"type\":"
                      "\"Text\",\"label\":\"two\"}]}"));
  SN_CHECK(recognition && sn_equal(recognition->text, "one\ntwo"),
           "only Text labels join in order");
  recognition = sn_recognition_read(&arena, encoded(&arena, "{}"));
  SN_CHECK(
      recognition && recognition->text.data && recognition->text.length == 0,
      "present empty document remains present");
  recognition =
      sn_recognition_read(&arena, encoded(&arena, "{\"elements\":false}"));
  SN_CHECK(!recognition, "wrong optional type rejects document");
  sn_arena_free(&arena);
  return sn_test_report("recognition");
}
