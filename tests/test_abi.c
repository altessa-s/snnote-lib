/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */
#include "sn_test.h"
#include "snnote/snnote.h"
int main(void) {
#if UINTPTR_MAX == UINT64_MAX
  SN_CHECK(sizeof(sn_string) == 16 && _Alignof(sn_string) == 8,
           "sn_string ABI layout");
  SN_CHECK(sizeof(sn_bytes) == 16 && _Alignof(sn_bytes) == 8,
           "sn_bytes ABI layout");
  SN_CHECK(sizeof(sn_point) == 16 && _Alignof(sn_point) == 8,
           "sn_point ABI layout");
  SN_CHECK(sizeof(sn_size) == 16 && _Alignof(sn_size) == 8,
           "sn_size ABI layout");
  SN_CHECK(sizeof(sn_rect) == 40 && _Alignof(sn_rect) == 8,
           "sn_rect ABI layout");
  SN_CHECK(sizeof(sn_error) == 272 && _Alignof(sn_error) == 8,
           "sn_error ABI layout");
  SN_CHECK(sizeof(sn_metadata_entry) == 32 && _Alignof(sn_metadata_entry) == 8,
           "sn_metadata_entry ABI layout");
  SN_CHECK(sizeof(sn_metadata) == 16 && _Alignof(sn_metadata) == 8,
           "sn_metadata ABI layout");
  SN_CHECK(sizeof(sn_bitmap) == 40 && _Alignof(sn_bitmap) == 8,
           "sn_bitmap ABI layout");
  SN_CHECK(sizeof(sn_ink) == 2 && _Alignof(sn_ink) == 1, "sn_ink ABI layout");
  SN_CHECK(sizeof(sn_palette) == 512 && _Alignof(sn_palette) == 1,
           "sn_palette ABI layout");
  SN_CHECK(sizeof(sn_layer_info) == 32 && _Alignof(sn_layer_info) == 8,
           "sn_layer_info ABI layout");
  SN_CHECK(sizeof(sn_layer) == 48 && _Alignof(sn_layer) == 8,
           "sn_layer ABI layout");
  SN_CHECK(sizeof(sn_stroke_point) == 24 && _Alignof(sn_stroke_point) == 8,
           "sn_stroke_point ABI layout");
  SN_CHECK(sizeof(sn_contour) == 16 && _Alignof(sn_contour) == 8,
           "sn_contour ABI layout");
  SN_CHECK(sizeof(sn_stroke) == 184 && _Alignof(sn_stroke) == 8,
           "sn_stroke ABI layout");
  SN_CHECK(sizeof(sn_text_box) == 128 && _Alignof(sn_text_box) == 8,
           "sn_text_box ABI layout");
  SN_CHECK(sizeof(sn_word) == 56 && _Alignof(sn_word) == 8,
           "sn_word ABI layout");
  SN_CHECK(sizeof(sn_element) == 48 && _Alignof(sn_element) == 8,
           "sn_element ABI layout");
  SN_CHECK(sizeof(sn_recognition) == 32 && _Alignof(sn_recognition) == 8,
           "sn_recognition ABI layout");
  SN_CHECK(sizeof(sn_page) == 216 && _Alignof(sn_page) == 8,
           "sn_page ABI layout");
  SN_CHECK(sizeof(sn_title) == 128 && _Alignof(sn_title) == 8,
           "sn_title ABI layout");
  SN_CHECK(sizeof(sn_keyword) == 120 && _Alignof(sn_keyword) == 8,
           "sn_keyword ABI layout");
  SN_CHECK(sizeof(sn_link) == 208 && _Alignof(sn_link) == 8,
           "sn_link ABI layout");
  SN_CHECK(sizeof(sn_highlight) == 88 && _Alignof(sn_highlight) == 8,
           "sn_highlight ABI layout");
  SN_CHECK(sizeof(sn_template) == 56 && _Alignof(sn_template) == 8,
           "sn_template ABI layout");
  SN_CHECK(sizeof(sn_notebook) == 736 && _Alignof(sn_notebook) == 8,
           "sn_notebook ABI layout");
  SN_CHECK(sizeof(sn_raster) == 32 && _Alignof(sn_raster) == 8,
           "sn_raster ABI layout");
#else
  puts("SKIP: layout baseline is for 64-bit targets");
#endif
  return sn_test_report("abi");
}
