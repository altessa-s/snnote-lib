/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#ifndef SN_FIXTURE_H
#define SN_FIXTURE_H
#include "sn_internal.h"
#include "sn_test.h"

typedef struct buffer {
  uint8_t data[32768];
  size_t size;
} buffer;
static void u32at(buffer* file_buffer, size_t at, uint32_t value) {
  for (size_t i = 0; i < 4; i++)
    file_buffer->data[at + i] = (uint8_t)(value >> (8 * i));
}
static void u32(buffer* file_buffer, uint32_t value) {
  u32at(file_buffer, file_buffer->size, value);
  file_buffer->size += 4;
}
static void append(buffer* file_buffer, const void* data, size_t length) {
  memcpy(file_buffer->data + file_buffer->size, data, length);
  file_buffer->size += length;
}
static void zeros(buffer* file_buffer, size_t length) {
  memset(file_buffer->data + file_buffer->size, 0, length);
  file_buffer->size += length;
}
static size_t block(buffer* file_buffer, const void* data, size_t length) {
  size_t at = file_buffer->size;
  u32(file_buffer, (uint32_t)length);
  append(file_buffer, data, length);
  return at;
}
static size_t tags(buffer* file_buffer, const char* status) {
  return block(file_buffer, status, strlen(status));
}
static buffer begin(void) {
  buffer file_buffer = {0};
  append(&file_buffer, "noteSN_FILE_VER_20230015", 24);
  return file_buffer;
}
static void finish(buffer* file_buffer, const char* footer) {
  size_t at = tags(file_buffer, footer);
  append(file_buffer, "tail", 4);
  u32(file_buffer, (uint32_t)at);
}
static buffer record(int id, uint32_t style) {
  buffer file_buffer = {0};
  zeros(&file_buffer, 208);
  u32at(&file_buffer, 0, 10);
  u32at(&file_buffer, 8, 400);
  u32at(&file_buffer, 28, 1);
  u32at(&file_buffer, 40, 5000);
  memcpy(file_buffer.data + 48, "others", 6);
  u32at(&file_buffer, 128, 15819);
  u32at(&file_buffer, 132, 11864);
  u32(&file_buffer, 0);
  u32(&file_buffer, 2);
  u32(&file_buffer, 1000);
  u32(&file_buffer, 1000);
  u32(&file_buffer, 2000);
  u32(&file_buffer, 2000);
  u32(&file_buffer, 2);
  const uint8_t pressure[] = {0, 8, 0, 8};
  append(&file_buffer, pressure, 4);
  zeros(&file_buffer, 16);
  size_t status = file_buffer.size;
  zeros(&file_buffer, 52);
  u32at(&file_buffer, status + 8, (uint32_t)id);
  u32(&file_buffer, 0);
  zeros(&file_buffer, 10);
  u32(&file_buffer, 0);
  u32(&file_buffer, 0);
  zeros(&file_buffer, 17);
  u32(&file_buffer, 1404);
  u32(&file_buffer, 1872);
  u32(&file_buffer, style);
  zeros(&file_buffer, 1);
  u32(&file_buffer, 4);
  append(&file_buffer, "none", 4);
  return file_buffer;
}
static buffer log_records(size_t count) {
  buffer file_buffer = {0};
  u32(&file_buffer, (uint32_t)count);
  for (size_t i = 0; i < count; i++) {
    buffer record_buffer = record((int)i + 1, 0);
    u32(&file_buffer, (uint32_t)record_buffer.size);
    append(&file_buffer, record_buffer.data, record_buffer.size);
  }
  return file_buffer;
}
static buffer notebook(const buffer* log) {
  buffer file_buffer = begin();
  size_t header_address =
      tags(&file_buffer, "<APPLY_EQUIPMENT:N6><FILE_ID:F1>");
  size_t log_address = log ? block(&file_buffer, log->data, log->size) : 0;
  char text[256];
  snprintf(text, sizeof text, "<TOTALPATH:%zu><PAGEID:P1>", log_address);
  size_t page_address = tags(&file_buffer, text);
  snprintf(text, sizeof text, "<FILE_FEATURE:%zu><PAGE1:%zu>", header_address,
           page_address);
  finish(&file_buffer, text);
  return file_buffer;
}
static sn_notebook* parse(const buffer* file_buffer) {
  sn_notebook* length = nullptr;
  sn_status status =
      sn_parse_notebook(file_buffer->data, file_buffer->size, &length, nullptr);
  SN_CHECK(status == SN_OK, "parse hand-built notebook: %s",
           sn_status_string(status));
  return length;
}

#endif
