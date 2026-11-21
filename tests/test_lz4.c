/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_internal.h"
#include "sn_test.h"

static void lz4(void) {
  sn_arena arena = {0};
  sn_bytes out;
  const uint8_t literal[] = {0x30, 'a', 'b', 'c'};
  SN_CHECK(sn_lz4(&arena, (sn_bytes){literal, sizeof literal}, 3, &out) &&
               !memcmp(out.data, "abc", 3),
           "literal");
  const uint8_t overlap[] = {0x14, 'a', 1, 0};
  SN_CHECK(sn_lz4(&arena, (sn_bytes){overlap, sizeof overlap}, 9, &out) &&
               !memcmp(out.data, "aaaaaaaaa", 9),
           "overlapping match");
  const uint8_t zero[] = {0, 0, 0};
  SN_CHECK(!sn_lz4(&arena, (sn_bytes){zero, sizeof zero}, 4, &out),
           "zero offset");
  SN_CHECK(!sn_lz4(&arena, (sn_bytes){literal, sizeof literal}, 4, &out),
           "exact output length");
  SN_CHECK(!sn_lz4(&arena, (sn_bytes){literal, sizeof literal}, 2000, &out),
           "ratio checked");
  for (size_t i = 0; i < sizeof overlap; i++)
    SN_CHECK(!sn_lz4(&arena, (sn_bytes){overlap, i}, 9, &out),
             "truncated match %zu", i);
  sn_arena_free(&arena);
}

/* Zero-literal sequences still require history; distance-one copies must
 * read the bytes just written, not a non-overlapping snapshot. */
static void match_sequences_and_length_boundaries(void) {
  sn_arena arena = {0};
  sn_bytes output;
  const uint8_t match_only[] = {0, 1, 0};
  SN_CHECK(
      !sn_lz4(&arena, (sn_bytes){match_only, sizeof match_only}, 4, &output),
      "match-only without history is rejected");
  const uint8_t subsequent_match[] = {0x10, 'x', 1, 0, 0, 1, 0};
  SN_CHECK(sn_lz4(&arena, (sn_bytes){subsequent_match, sizeof subsequent_match},
                  9, &output) &&
               !memcmp(output.data, "xxxxxxxxx", 9),
           "zero-literal match uses existing history");
  const uint8_t before_start[] = {0x10, 'x', 2, 0};
  SN_CHECK(!sn_lz4(&arena, (sn_bytes){before_start, sizeof before_start}, 5,
                   &output),
           "offset before output start rejected");
  const uint8_t long_run[] = {0x1f, 'x', 1, 0, 255, 255, 0};
  SN_CHECK(sn_lz4(&arena, (sn_bytes){long_run, sizeof long_run}, 530, &output),
           "consecutive 255 length extensions");
  SN_CHECK(!sn_lz4(&arena, (sn_bytes){long_run, sizeof long_run}, 529, &output),
           "output one byte short rejected");
  SN_CHECK(!sn_lz4(&arena, (sn_bytes){long_run, sizeof long_run}, 531, &output),
           "output one byte long rejected");
  SN_CHECK(
      !sn_lz4(&arena, (sn_bytes){long_run, sizeof long_run - 1}, 530, &output),
      "input ending inside match length rejected");
  const uint8_t missing_literal_length[] = {0xf0, 255};
  SN_CHECK(
      !sn_lz4(&arena,
              (sn_bytes){missing_literal_length, sizeof missing_literal_length},
              270, &output),
      "input ending inside literal length rejected");
  sn_arena_free(&arena);
}

int main(void) {
  lz4();
  match_sequences_and_length_boundaries();
  return sn_test_report("lz4");
}
