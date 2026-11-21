/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_encoded.h"

int main(void) {
  sn_arena arena = {0};
  sn_notebook notebook = {0};
  sn_highlights(
      &arena,
      encoded(&arena,
              "{\"2\":[{\"time\":1250,\"rnRectList\":[{\"left\":10,\"top\":20,"
              "\"right\":40,\"bottom\":60}]}],\"0\":[{}]}"),
      &notebook);
  SN_CHECK(notebook.highlight_count == 2 &&
               notebook.highlights[0].page_number == 1 &&
               notebook.highlights[1].page_number == 3,
           "page keys sort numerically and become one-based");
  if (notebook.highlight_count == 2) {
    const sn_highlight* highlight = notebook.highlights + 1;
    SN_CHECK(highlight->has_date && highlight->date == 1.25,
             "milliseconds become seconds");
    SN_CHECK(highlight->rect_count == 1 && highlight->rects[0].width == 30 &&
                 highlight->rects[0].height == 40,
             "screen corners become extents");
  }
  notebook = (sn_notebook){0};
  sn_highlights(&arena,
                encoded(&arena,
                        "{\"1\":[{\"annotationContent\":\"first\"}],\"01\":[{"
                        "\"annotationContent\":\"second\"}],\"1\":[{"
                        "\"annotationContent\":\"discard\"}]}"),
                &notebook);
  SN_CHECK(notebook.highlight_count == 2, "first repeated key array only");
  if (notebook.highlight_count == 2) {
    SN_CHECK(sn_equal(notebook.highlights[0].comment, "first"),
             "first numeric tie retains file order");
    SN_CHECK(sn_equal(notebook.highlights[1].comment, "second"),
             "second numeric tie retains file order");
  }
  const char* bad[] = {
      "{\"0\":[{\"mupdfRectList\":[{\"x0\":-1e308,\"y0\":0,\"x1\":1e308,\"y1\":"
      "1}]}]}",
      "{\"0\":[{\"rnRectList\":[{\"left\":0,\"top\":-1e308,\"right\":1,"
      "\"bottom\":1e308}]}]}"};
  for (size_t i = 0; i < 2; i++) {
    notebook = (sn_notebook){0};
    sn_highlights(&arena, encoded(&arena, bad[i]), &notebook);
    SN_CHECK(notebook.highlight_count == 0,
             "overflowing corner difference rejected");
  }
  sn_arena_free(&arena);
  return sn_test_report("highlights");
}
