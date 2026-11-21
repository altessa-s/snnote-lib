/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_internal.h"
#include "sn_test.h"

static void metadata(void) {
  sn_arena arena = {0};
  const char* text =
      "junk<A:first><B:colon:kept><A:second><:empty><bad<Z:yes><cut:off";
  sn_metadata metadata =
      sn_metadata_read(&arena, (sn_bytes){(const uint8_t*)text, strlen(text)});
  SN_CHECK(metadata.count == 4, "complete nonempty keys");
  const sn_string* value = sn_metadata_get(&metadata, "A");
  SN_CHECK(value && sn_equal(*value, "first"), "first value");
  size_t i = 0;
  value = sn_metadata_next(&metadata, "A", &i);
  SN_CHECK(value && sn_equal(*value, "first"), "first repeat");
  value = sn_metadata_next(&metadata, "A", &i);
  SN_CHECK(value && sn_equal(*value, "second"), "second repeat");
  SN_CHECK(!sn_metadata_next(&metadata, "A", &i), "end");
  const uint8_t broken[] = {0xf0, 0x90, 0x80, 'x', 0xff, 0, 0xc3, 0xa9};
  sn_string repaired = sn_text(&arena, broken, sizeof broken);
  SN_CHECK(repaired.length == 10, "maximal invalid subparts and embedded NUL");
  sn_arena_free(&arena);
}

static void fixed_fields(void) {
  sn_arena arena = {0};
  const char* text =
      "<TITLELEVEL: "
      "\t2\xc2\xa0><TITLELEVEL:9><PAGEID:><PAGEID:later><UNKNOWN:kept><"
      "LINKTYPE:bad>";
  sn_metadata metadata =
      sn_metadata_read(&arena, (sn_bytes){(const uint8_t*)text, strlen(text)});
  sn_metadata fields = sn_fields(&arena, 24, metadata);
  SN_CHECK(metadata.count == 6 && fields.count == 3,
           "fixed view preserves original entries");
  SN_CHECK(sn_field_number(fields, "TITLELEVEL", 0) ==
                   sn_number(metadata, "TITLELEVEL", 0) &&
               sn_field_number(fields, "TITLELEVEL", 0) == 2,
           "trim numeric whitespace and keep first duplicate");
  sn_string id = sn_field_value(fields, "PAGEID");
  SN_CHECK(id.data && !id.length, "first empty field remains present");
  SN_CHECK(sn_field_number(fields, "LINKTYPE", 7) == 7 &&
               !sn_field_value(fields, "UNKNOWN").data,
           "invalid number and unknown field");
  SN_CHECK(sn_fields(&arena, 24, metadata).entries == fields.entries,
           "projection reused");
  sn_arena_free(&arena);
}

static void unicode_whitespace(void) {
  const uint32_t whites[] = {9,      32,     0xa0,   0x1680, 0x2000,
                             0x2001, 0x2002, 0x2003, 0x2004, 0x2005,
                             0x2006, 0x2007, 0x2008, 0x2009, 0x200a,
                             0x200b, 0x202f, 0x205f, 0x3000};
  const uint32_t lines[] = {10, 11, 12, 13, 0x85, 0x2028, 0x2029};
  for (size_t group = 0; group < 2; group++) {
    const uint32_t* members = group ? lines : whites;
    size_t count =
        group ? sizeof lines / sizeof *lines : sizeof whites / sizeof *whites;
    for (size_t index = 0; index < count; index++) {
      uint32_t c = members[index];
      char bytes[9];
      size_t length = 0;
      if (c < 128)
        bytes[length++] = (char)c;
      else if (c < 2048) {
        bytes[length++] = (char)(0xc0 | (c >> 6));
        bytes[length++] = (char)(0x80 | (c & 63));
      } else {
        bytes[length++] = (char)(0xe0 | (c >> 12));
        bytes[length++] = (char)(0x80 | ((c >> 6) & 63));
        bytes[length++] = (char)(0x80 | (c & 63));
      }
      bytes[length] = 'x';
      memcpy(bytes + length + 1, bytes, length);
      sn_string text = {bytes, length * 2 + 1};
      sn_string trimmed = sn_trim(text, true);
      SN_CHECK(trimmed.length == 1 && trimmed.data[0] == 'x',
               "trim scalar U+%04x", (unsigned)c);
      trimmed = sn_trim(text, false);
      SN_CHECK(trimmed.length == (group ? text.length : 1),
               "whitespace-only U+%04x", (unsigned)c);
    }
  }
}

int main(void) {
  unicode_whitespace();
  metadata();
  fixed_fields();
  return sn_test_report("metadata");
}
