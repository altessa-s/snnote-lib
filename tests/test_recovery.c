/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_fixture.h"
#include "sn_internal.h"
#include "sn_test.h"

static void container(void) {
  buffer file_buffer = notebook(nullptr);
  sn_notebook* sentinel = (sn_notebook*)(uintptr_t)1;
  SN_CHECK(sn_parse_notebook(file_buffer.data, 20, &sentinel, nullptr) ==
                   SN_ERR_UNEXPECTED_EOF &&
               sentinel == (void*)(uintptr_t)1,
           "output unchanged");
  u32at(&file_buffer, file_buffer.size - 4, UINT32_MAX);
  SN_CHECK(sn_parse_notebook(file_buffer.data, file_buffer.size, &sentinel,
                             nullptr) != SN_OK,
           "bad footer");
  file_buffer = begin();
  size_t header_address = tags(&file_buffer, "<APPLY_EQUIPMENT:N6>");
  size_t old = tags(&file_buffer, "<PAGEID:old>");
  size_t new_page = tags(&file_buffer, "<PAGEID:new>");
  char text[512];
  snprintf(text, sizeof text,
           "<FILE_FEATURE:%zu><PAGE1:%zu><PAGE1:%zu><PAGE2:99999999>",
           header_address, old, new_page);
  finish(&file_buffer, text);
  sn_notebook* notebook_value = parse(&file_buffer);
  if (notebook_value) {
    SN_CHECK(notebook_value->page_count == 1 &&
                 notebook_value->skipped_pages == 1 &&
                 sn_equal(notebook_value->pages[0].id, "new"),
             "last page and skipped page");
    sn_notebook_free(notebook_value);
  }
  file_buffer = begin();
  header_address = tags(&file_buffer, "<APPLY_EQUIPMENT:N6>");
  size_t tag_text =
      tags(&file_buffer, "<TITLERECT:10,20,30,40><TITLESTYLE:1000254>");
  size_t t2 = tags(&file_buffer, "<TITLERECT:10,20,30,40><TITLESTYLE:1201000>");
  snprintf(text, sizeof text,
           "<FILE_FEATURE:%zu><TITLE_00010001:%zu><TITLE_00010001:%zu>",
           header_address, tag_text, t2);
  finish(&file_buffer, text);
  notebook_value = parse(&file_buffer);
  if (notebook_value) {
    SN_CHECK(notebook_value->title_count == 1 &&
                 notebook_value->titles[0].style == 1201000,
             "resaved title");
    sn_notebook_free(notebook_value);
  }
}
static buffer blank(size_t total) {
  buffer file_buffer = {0};
  while (total >= 16384) {
    const uint8_t run[] = {0x62, 255};
    append(&file_buffer, run, 2);
    total -= 16384;
  }
  while (total) {
    size_t notebook_value = total > 128 ? 128 : total;
    uint8_t run[] = {0x62, (uint8_t)(notebook_value - 1)};
    append(&file_buffer, run, 2);
    total -= notebook_value;
  }
  return file_buffer;
}
static void engine(void) {
  buffer file_buffer = begin();
  size_t header_address = tags(&file_buffer, "<APPLY_EQUIPMENT:Unknown>");
  buffer pixels = blank(1920u * 2560u);
  size_t bitmap = block(&file_buffer, pixels.data, pixels.size);
  char tag_text[1024];
  snprintf(tag_text, sizeof tag_text, "<LAYERBITMAP:%zu>", bitmap);
  size_t layer = tags(&file_buffer, tag_text);
  snprintf(tag_text, sizeof tag_text, "<MAINLAYER:%zu><LAYERSEQ:MAINLAYER>",
           layer);
  size_t page_address = tags(&file_buffer, tag_text);
  snprintf(tag_text, sizeof tag_text, "<FILE_FEATURE:%zu><PAGE1:%zu>",
           header_address, page_address);
  finish(&file_buffer, tag_text);
  sn_notebook* notebook_value = parse(&file_buffer);
  if (notebook_value) {
    SN_CHECK(notebook_value->equipment == SN_EQUIPMENT_OTHER &&
                 notebook_value->sheet.width == 1920,
             "unknown equipment measured");
    sn_notebook_free(notebook_value);
  }
  file_buffer = begin();
  header_address = tags(&file_buffer, "<APPLY_EQUIPMENT:N6>");
  bitmap = block(&file_buffer, pixels.data, pixels.size);
  snprintf(tag_text, sizeof tag_text, "<LAYERBITMAP:%zu>", bitmap);
  layer = tags(&file_buffer, tag_text);
  snprintf(tag_text, sizeof tag_text, "<MAINLAYER:%zu><LAYERSEQ:MAINLAYER>",
           layer);
  page_address = tags(&file_buffer, tag_text);
  snprintf(tag_text, sizeof tag_text,
           "<MAINLAYER:%zu><PAGEMACHINETYPE:5><ORIENTATION:1090>", layer);
  size_t p2 = tags(&file_buffer, tag_text);
  snprintf(tag_text, sizeof tag_text,
           "<FILE_FEATURE:%zu><PAGE1:%zu><PAGE2:%zu>", header_address,
           page_address, p2);
  finish(&file_buffer, tag_text);
  notebook_value = parse(&file_buffer);
  if (notebook_value) {
    SN_CHECK(notebook_value->pages[0].size.width == 1920 &&
                 notebook_value->pages[1].size.width == 2560 &&
                 notebook_value->pages[1].size.height == 1920,
             "page measured and named sheets");
    sn_notebook_free(notebook_value);
  }
  file_buffer = begin();
  header_address = tags(&file_buffer, "<APPLY_EQUIPMENT:N6>");
  const uint8_t jpeg[] = {255, 216, 255, 0};
  size_t cover = block(&file_buffer, jpeg, sizeof jpeg);
  snprintf(tag_text, sizeof tag_text, "<FILE_FEATURE:%zu><COVER_2:%zu>",
           header_address, cover);
  finish(&file_buffer, tag_text);
  notebook_value = parse(&file_buffer);
  if (notebook_value) {
    SN_CHECK(notebook_value->cover_type == 2 && notebook_value->cover &&
                 notebook_value->cover->encoding == SN_ENCODING_JPEG,
             "typed cover");
    sn_raster* record_buffer = nullptr;
    SN_CHECK(sn_bitmap_raster(notebook_value->cover, 8, 6, nullptr,
                              &record_buffer, nullptr) == SN_ERR_IMAGE_ENCODING,
             "JPEG handed over");
    sn_notebook_free(notebook_value);
  }
  buffer log = {0};
  u32(&log, 4);
  uint32_t styles[] = {0, 5000000, 25000000, 24000001};
  for (size_t i = 0; i < 4; i++) {
    buffer record_buffer = record((int)i + 1, styles[i]);
    u32(&log, (uint32_t)record_buffer.size);
    append(&log, record_buffer.data, record_buffer.size);
  }
  file_buffer = notebook(&log);
  notebook_value = parse(&file_buffer);
  if (notebook_value) {
    const sn_page* page = notebook_value->pages;
    SN_CHECK(page->drawn_stroke_count == 1 && page->text_box_count == 1 &&
                 page->text_boxes[0].id == 2,
             "text boxes listed not drawn");
    sn_notebook_free(notebook_value);
  }
  /* A truncated contour loses only the outline, not the centreline. */
  buffer record_buffer = record(1, 0);
  record_buffer.size = 326;
  u32at(&record_buffer, 322, UINT32_MAX);
  log = (buffer){0};
  u32(&log, 1);
  u32(&log, (uint32_t)record_buffer.size);
  append(&log, record_buffer.data, record_buffer.size);
  file_buffer = notebook(&log);
  notebook_value = parse(&file_buffer);
  if (notebook_value) {
    SN_CHECK(notebook_value->pages[0].stroke_count == 1, "outline recovery");
    sn_notebook_free(notebook_value);
  }
}
/* Missing and zero orientation use portrait, alongside the two explicit
 * portrait codes; both explicit landscape codes swap axes. */
static void orientation_codes(void) {
  const char* values[] = {"",
                          "<ORIENTATION:0>",
                          "<ORIENTATION:1000>",
                          "<ORIENTATION:1180>",
                          "<ORIENTATION:1090>",
                          "<ORIENTATION:1270>"};
  for (size_t index = 0; index < sizeof values / sizeof *values; index++) {
    buffer file = begin();
    size_t header = tags(&file, "<APPLY_EQUIPMENT:N6>");
    size_t page = tags(&file, values[index]);
    char footer[128];
    snprintf(footer, sizeof footer, "<FILE_FEATURE:%zu><PAGE1:%zu>", header,
             page);
    finish(&file, footer);
    sn_notebook* parsed = parse(&file);
    if (parsed) {
      SN_CHECK(
          parsed->page_count == 1 && parsed->pages[0].orientation ==
                                         (index < 4 ? SN_ORIENTATION_PORTRAIT
                                                    : SN_ORIENTATION_LANDSCAPE),
          "orientation code %zu", index);
      sn_notebook_free(parsed);
    }
  }
}

int main(void) {
  container();
  orientation_codes();
  engine();
  return sn_test_report("recovery");
}
