/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#ifndef SN_ENCODED_H
#define SN_ENCODED_H
#include "sn_internal.h"
#include "sn_test.h"

static sn_bytes encoded(sn_arena* arena, const char* text) {
  const char* alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t length = strlen(text), size = (length + 2) / 3 * 4;
  uint8_t* bytes = sn_alloc(arena, size, 1);
  size_t pos = 0;
  for (size_t i = 0; i < length; i += 3) {
    unsigned bits = (unsigned)(unsigned char)text[i] << 16;
    if (i + 1 < length) bits |= (unsigned)(unsigned char)text[i + 1] << 8;
    if (i + 2 < length) bits |= (unsigned char)text[i + 2];
    bytes[pos++] = (uint8_t)alphabet[(bits >> 18) & 63];
    bytes[pos++] = (uint8_t)alphabet[(bits >> 12) & 63];
    bytes[pos++] = i + 1 < length ? (uint8_t)alphabet[(bits >> 6) & 63] : '=';
    bytes[pos++] = i + 2 < length ? (uint8_t)alphabet[bits & 63] : '=';
  }
  return (sn_bytes){bytes, size};
}
#endif
