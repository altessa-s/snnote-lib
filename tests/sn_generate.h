/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */
#ifndef SN_GENERATE_H
#define SN_GENERATE_H
#include "sn_internal.h"

typedef struct generated {
  uint8_t* data;
  size_t size, capacity;
} generated;
static void gen_append(generated* buffer, const void* data, size_t size) {
  if (size > SIZE_MAX - buffer->size) abort();
  if (buffer->size + size > buffer->capacity) {
    size_t capacity = buffer->size + size + 4096;
    void* grown = realloc(buffer->data, capacity);
    if (!grown) abort();
    buffer->data = grown;
    buffer->capacity = capacity;
  }
  if (data)
    memcpy(buffer->data + buffer->size, data, size);
  else
    memset(buffer->data + buffer->size, 0, size);
  buffer->size += size;
}
static void gen_u32(generated* buffer, uint32_t value) {
  uint8_t bytes[4];
  for (size_t index = 0; index < 4; index++)
    bytes[index] = (uint8_t)(value >> (8 * index));
  gen_append(buffer, bytes, 4);
}
static size_t gen_block(generated* buffer, const void* data, size_t size) {
  size_t address = buffer->size;
  gen_u32(buffer, (uint32_t)size);
  gen_append(buffer, data, size);
  return address;
}
static size_t gen_tags(generated* buffer, const char* text) {
  return gen_block(buffer, text, strlen(text));
}
static generated gen_begin(void) {
  generated file = {0};
  gen_append(&file, "noteSN_FILE_VER_20230015", 24);
  gen_tags(&file, "<APPLY_EQUIPMENT:N5>");
  return file;
}
static void gen_finish(generated* file, generated* footer) {
  gen_append(footer, "<FILE_FEATURE:24>", 17);
  size_t address = gen_block(file, footer->data, footer->size);
  gen_append(file, "tail", 4);
  gen_u32(file, (uint32_t)address);
  free(footer->data);
}
static void gen_key(generated* footer, const char* prefix, size_t number,
                    size_t address) {
  char text[128];
  int length =
      snprintf(text, sizeof text, "<%s%zu:%zu>", prefix, number, address);
  gen_append(footer, text, (size_t)length);
}
static generated gen_record(size_t points, size_t rings) {
  generated record = {0};
  gen_append(&record, nullptr, 208);
  record.data[0] = 16;
  record.data[40] = 0x88;
  record.data[41] = 0x13;
  memcpy(record.data + 48, "others", 6);
  gen_u32(&record, 0);
  gen_u32(&record, (uint32_t)points);
  for (size_t index = 0; index < points; index++) {
    gen_u32(&record, 100);
    gen_u32(&record, 100);
  }
  gen_append(&record, nullptr, 20);
  gen_u32(&record, 0);
  gen_u32(&record, 0);
  gen_u32(&record, 1);
  gen_append(&record, nullptr, 40);
  gen_u32(&record, 0);
  gen_append(&record, nullptr, 10);
  gen_u32(&record, (uint32_t)rings);
  gen_append(&record, nullptr, rings * 4);
  return record;
}
static void gen_length(generated* packed, size_t length) {
  uint8_t byte = 255;
  while (length >= 255) {
    gen_append(packed, &byte, 1);
    length -= 255;
  }
  byte = (uint8_t)length;
  gen_append(packed, &byte, 1);
}
static generated gen_literal(const generated* raw) {
  generated packed = {0};
  uint8_t token = (uint8_t)((raw->size < 15 ? raw->size : 15) << 4);
  gen_append(&packed, &token, 1);
  if (raw->size >= 15) gen_length(&packed, raw->size - 15);
  gen_append(&packed, raw->data, raw->size);
  return packed;
}
static size_t gen_log(generated* file, const generated* packed, size_t count,
                      size_t expanded) {
  generated log = {0};
  gen_u32(&log, UINT32_MAX);
  gen_u32(&log, (uint32_t)count);
  gen_u32(&log, (uint32_t)expanded);
  gen_u32(&log, (uint32_t)packed->size);
  gen_append(&log, packed->data, packed->size);
  size_t address = gen_block(file, log.data, log.size);
  free(log.data);
  return address;
}
static size_t gen_base64(generated* file, const generated* raw) {
  const char* alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  generated encoded = {0};
  for (size_t index = 0; index < raw->size; index += 3) {
    uint32_t bits = (uint32_t)raw->data[index] << 16;
    if (index + 1 < raw->size) bits |= (uint32_t)raw->data[index + 1] << 8;
    if (index + 2 < raw->size) bits |= raw->data[index + 2];
    uint8_t bytes[] = {
        (uint8_t)alphabet[(bits >> 18) & 63],
        (uint8_t)alphabet[(bits >> 12) & 63],
        index + 1 < raw->size ? (uint8_t)alphabet[(bits >> 6) & 63] : '=',
        index + 2 < raw->size ? (uint8_t)alphabet[bits & 63] : '='};
    gen_append(&encoded, bytes, 4);
  }
  size_t address = gen_block(file, encoded.data, encoded.size);
  free(encoded.data);
  return address;
}
static generated gen_hostile(size_t shape) {
  generated file = gen_begin(), footer = {0};
  size_t page = 0;
  char text[256];
  if (shape == 0) {
    generated record = gen_record(131072, 0), log = {0};
    gen_u32(&log, 1);
    gen_block(&log, record.data, record.size);
    size_t address = gen_block(&file, log.data, log.size);
    snprintf(text, sizeof text, "<PAGEID:x><TOTALPATH:%zu>", address);
    page = gen_tags(&file, text);
    for (size_t index = 0; index < 1000; index++)
      gen_key(&footer, "PAGE", index + 1, page);
    free(record.data);
    free(log.data);
  } else if (shape == 1) {
    generated metadata = {0};
    gen_append(&metadata, "<PAD:", 5);
    gen_append(&metadata, nullptr, 1048576);
    memset(metadata.data + 5, 'x', 1048576);
    gen_append(&metadata, "><TITLERECT:1,2,3,4>", 19);
    size_t address = gen_block(&file, metadata.data, metadata.size);
    page = gen_tags(&file, "<PAGEID:x>");
    gen_key(&footer, "PAGE", 1, page);
    for (size_t index = 0; index < 300; index++)
      gen_key(&footer, "TITLE_", index + 10000, address);
    free(metadata.data);
  } else if (shape == 2 || shape == 3 || shape == 5 || shape == 6) {
    generated packed = {0};
    size_t expanded, count;
    if (shape != 3) {
      size_t rings = shape == 2 ? 20000000 : shape == 5 ? 100000 : 20000;
      generated record = gen_record(0, 0), prefix = {0};
      record.size -= 4;
      gen_u32(&record, (uint32_t)rings);
      gen_u32(&prefix, (uint32_t)(record.size + rings * 4));
      gen_append(&prefix, record.data, record.size);
      gen_append(&prefix, nullptr, 1);
      uint8_t token = 0xff;
      gen_append(&packed, &token, 1);
      gen_length(&packed, prefix.size - 15);
      gen_append(&packed, prefix.data, prefix.size);
      const uint8_t offset[] = {1, 0};
      gen_append(&packed, offset, 2);
      gen_length(&packed, rings * 4 - 1 - 4 - 15);
      expanded = prefix.size + rings * 4 - 1;
      count = 1;
      free(prefix.data);
      free(record.data);
    } else {
      const uint8_t prefix[] = {0x1f, 0, 1, 0};
      gen_append(&packed, prefix, 4);
      gen_length(&packed, 255 * 1000000);
      expanded = 20 + 255 * 1000000;
      count = expanded / 4;
    }
    size_t logs = shape == 6 ? 100 : 1;
    for (size_t index = 0; index < logs; index++) {
      size_t address = gen_log(&file, &packed, count, expanded);
      snprintf(text, sizeof text, "<TOTALPATH:%zu>", address);
      page = gen_tags(&file, text);
      gen_key(&footer, "PAGE", index + 1, page);
    }
    free(packed.data);
  } else {
    generated raw = {0};
    const char* prefix = "{\"elements\":[{\"type\":\"Text\",\"words\":[";
    gen_append(&raw, prefix, strlen(prefix));
    for (size_t index = 0; index < 1000000; index++)
      gen_append(&raw, index ? ",0" : "0", index ? 2 : 1);
    gen_append(&raw, "]}]}", 4);
    size_t address = gen_base64(&file, &raw);
    snprintf(text, sizeof text, "<RECOGNTEXT:%zu>", address);
    page = gen_tags(&file, text);
    gen_key(&footer, "PAGE", 1, page);
    free(raw.data);
  }
  if (shape == 2 || shape == 3 || shape == 5) {
    size_t good = gen_tags(&file, "<PAGEID:good>");
    gen_key(&footer, "PAGE", 2, good);
  }
  gen_finish(&file, &footer);
  return file;
}
/* Small complete containers keep footer references reachable during mutation.
 */
static generated gen_seed(size_t kind) {
  generated file = gen_begin(), footer = {0};
  char text[256];
  size_t page;
  if (kind < 2) {
    generated record = gen_record(2, 0), records = {0};
    gen_block(&records, record.data, record.size);
    size_t address;
    if (kind == 0) {
      generated log = {0};
      gen_u32(&log, 1);
      gen_append(&log, records.data, records.size);
      address = gen_block(&file, log.data, log.size);
      free(log.data);
    } else {
      generated packed = gen_literal(&records);
      address = gen_log(&file, &packed, 1, records.size);
      free(packed.data);
    }
    snprintf(text, sizeof text, "<TOTALPATH:%zu>", address);
    page = gen_tags(&file, text);
    free(record.data);
    free(records.data);
  } else if (kind < 5) {
    const char* metadata[] = {
        "<TITLERECT:1,2,3,4><TITLELEVEL:2>",
        "<KEYWORD: keyword ><KEYWORDRECT:1,2,3,4>",
        "<LINKTYPE:4><LINKFILE:aHR0cHM6Ly9leGFtcGxlLmNvbQ==><SHOWTEXT:77u/"
        "bGluaw==>"};
    const char* prefix[] = {"TITLE_", "KEYWORD_", "LINK_"};
    size_t address = gen_tags(&file, metadata[kind - 2]);
    gen_key(&footer, prefix[kind - 2], 10001, address);
    page = gen_tags(&file, "<PAGEID:p>");
  } else {
    const char* json =
        kind == 5 ? "{\"0\":[{\"annotationContent\":\"mark\",\"time\":1000,"
                    "\"rnRectList\":[{\"left\":1,\"top\":2,\"right\":3,"
                    "\"bottom\":4}]}]}"
                  : "{\"elements\":[{\"type\":\"Text\",\"label\":\"text\","
                    "\"words\":[{\"label\":\"text\",\"bounding-box\":{\"x\":1,"
                    "\"y\":2,\"width\":3,\"height\":4}}]}]}";
    generated raw = {0};
    gen_append(&raw, json, strlen(json));
    size_t address = gen_base64(&file, &raw);
    free(raw.data);
    if (kind == 5) {
      memcpy(file.data, "mark", 4);
      snprintf(text, sizeof text, "<APPLY_EQUIPMENT:N5><HIGHLIGHTINFO:%zu>",
               address);
      size_t header = gen_tags(&file, text);
      snprintf(text, sizeof text, "<FILE_FEATURE:%zu>", header);
      gen_append(&footer, text, strlen(text));
      page = gen_tags(&file, "<PAGEID:p>");
    } else {
      snprintf(text, sizeof text, "<RECOGNTEXT:%zu>", address);
      page = gen_tags(&file, text);
    }
  }
  gen_key(&footer, "PAGE", 1, page);
  gen_finish(&file, &footer);
  return file;
}
#endif
