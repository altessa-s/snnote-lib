/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_fixture.h"

static void records(void) {
  buffer log = log_records(3);
  buffer record_buffer = record(1, 0);
  u32at(&log, 4 + record_buffer.size + 4, 20);
  /* Rebuild with an independently sized bad record so that the next length
   * remains readable. */
  log = (buffer){0};
  u32(&log, 3);
  u32(&log, (uint32_t)record_buffer.size);
  append(&log, record_buffer.data, record_buffer.size);
  u32(&log, 20);
  zeros(&log, 20);
  record_buffer = record(3, 0);
  u32(&log, (uint32_t)record_buffer.size);
  append(&log, record_buffer.data, record_buffer.size);
  buffer file_buffer = notebook(&log);
  sn_notebook* notebook_value = parse(&file_buffer);
  if (notebook_value) {
    SN_CHECK(notebook_value->pages[0].stroke_count == 2 &&
                 notebook_value->pages[0].skipped_strokes == 1,
             "step over bad record");
    SN_CHECK(notebook_value->pages[0].strokes[1].id == 3, "next record");
    sn_notebook_free(notebook_value);
  }
  log = log_records(2);
  u32at(&log, 0, 5);
  file_buffer = notebook(&log);
  notebook_value = parse(&file_buffer);
  if (notebook_value) {
    SN_CHECK(notebook_value->pages[0].stroke_count == 2 &&
                 notebook_value->pages[0].skipped_strokes == 3,
             "truncated log");
    sn_notebook_free(notebook_value);
  }
  log = log_records(1);
  u32at(&log, 8 + 212, UINT32_MAX);
  file_buffer = notebook(&log);
  notebook_value = parse(&file_buffer);
  if (notebook_value) {
    SN_CHECK(notebook_value->pages[0].stroke_count == 0 &&
                 notebook_value->pages[0].skipped_strokes == 1,
             "huge point count");
    sn_notebook_free(notebook_value);
  }
  log = log_records(1);
  file_buffer = notebook(&log);
  notebook_value = parse(&file_buffer);
  if (notebook_value) {
    const sn_stroke* stroke_value = notebook_value->pages[0].strokes;
    SN_CHECK(stroke_value->points[0].position.x ==
                 1404.0 - 1000.0 / (11864.0 / 1404.0),
             "x arithmetic");
    SN_CHECK(stroke_value->points[0].position.y == 1000.0 / (15819.0 / 1872.0),
             "y arithmetic");
    SN_CHECK(
        stroke_value->points[0].pressure == 2048 && stroke_value->has_style,
        "pressure and tail");
    sn_notebook_free(notebook_value);
  }
  /* A literal-only raw LZ4 block exercises the compressed envelope without
   * bringing a compressor into the test dependencies. */
  log = log_records(5);
  buffer packed = {0};
  append(&packed, "\xf0", 1);
  size_t literal = log.size - 4 - 15;
  while (literal >= 255) {
    append(&packed, "\xff", 1);
    literal -= 255;
  }
  uint8_t last = (uint8_t)literal;
  append(&packed, &last, 1);
  append(&packed, log.data + 4, log.size - 4);
  buffer compressed = {0};
  u32(&compressed, UINT32_MAX);
  u32(&compressed, 5);
  u32(&compressed, (uint32_t)log.size - 4);
  u32(&compressed, (uint32_t)packed.size);
  append(&compressed, packed.data, packed.size);
  file_buffer = notebook(&compressed);
  notebook_value = parse(&file_buffer);
  if (notebook_value) {
    SN_CHECK(notebook_value->pages[0].stroke_count == 5 &&
                 !notebook_value->pages[0].skipped_strokes,
             "compressed records");
    sn_notebook_free(notebook_value);
  }
  u32at(&compressed, 8, INT32_MAX);
  file_buffer = notebook(&compressed);
  notebook_value = parse(&file_buffer);
  if (notebook_value) {
    SN_CHECK(notebook_value->pages[0].skipped_strokes == 1 &&
                 !notebook_value->pages[0].stroke_count,
             "lying compressed length");
    sn_notebook_free(notebook_value);
  }
}

/* Each tail layout places its sheet signature at a different offset. Two
 * entries ensure the 20-byte and both 16-byte candidates cannot coincide. */
static void tail_layouts_and_zero_ranges(void) {
  const size_t strides[] = {20, 16, 16};
  const size_t prefixes[] = {4, 8, 4};
  for (size_t layout = 0; layout < 3; layout++) {
    buffer stroke = record(7, 0);
    stroke.size = 326;
    u32(&stroke, 2);
    zeros(&stroke, prefixes[layout] - 4 + 2 * strides[layout]);
    zeros(&stroke, 13);
    u32(&stroke, 0);
    u32(&stroke, 1404);
    u32(&stroke, 1872);
    u32(&stroke, 1000254);
    zeros(&stroke, 1);
    u32(&stroke, 4);
    append(&stroke, "none", 4);
    u32at(&stroke, 128, 0);
    u32at(&stroke, 132, 0);
    buffer log = {0};
    u32(&log, 1);
    u32(&log, (uint32_t)stroke.size);
    append(&log, stroke.data, stroke.size);
    buffer file = notebook(&log);
    sn_notebook* parsed = parse(&file);
    if (parsed && parsed->pages[0].stroke_count) {
      const sn_stroke* result = parsed->pages[0].strokes;
      SN_CHECK(result->has_style && result->style == 1000254,
               "tail candidate %zu", layout);
      SN_CHECK(result->points[0].position.x == 404 &&
                   result->points[0].position.y == 1000,
               "zero y range uses one, zero x range reuses y scale");
      SN_CHECK(result->id == 7 && result->pen == 10 &&
                   result->page_number == 1 && result->role_raw == 5000,
               "record header fields retain their offsets");
    } else
      SN_CHECK(false, "stroke retained");
    sn_notebook_free(parsed);
  }
  buffer stroke = record(8, 0);
  u32at(&stroke, 132, 0);
  buffer log = {0};
  u32(&log, 1);
  u32(&log, (uint32_t)stroke.size);
  append(&log, stroke.data, stroke.size);
  buffer file = notebook(&log);
  sn_notebook* parsed = parse(&file);
  if (parsed && parsed->pages[0].stroke_count) {
    const sn_point point = parsed->pages[0].strokes[0].points[0].position;
    SN_CHECK(point.x == 1404 - 1000 / (15819.0 / 1872),
             "zero x range reuses nonzero y scale");
  }
  sn_notebook_free(parsed);
}

int main(void) {
  records();
  tail_layouts_and_zero_ranges();
  return sn_test_report("strokes");
}
