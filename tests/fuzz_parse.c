/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_composition.h"
#include "sn_internal.h"
/* Volatile accumulation keeps the traversal visible to the optimizer: a
 * successful parse must expose every returned array safely. */
static volatile uint64_t touched;
static void bytes(sn_bytes bitmap) {
  for (size_t i = 0; i < bitmap.size; i++) touched += bitmap.data[i];
}
static void text(sn_string status) {
  bytes((sn_bytes){(const uint8_t*)status.data, status.length});
}
static void metadata(sn_metadata m) {
  for (size_t i = 0; i < m.count; i++) {
    text(m.entries[i].key);
    text(m.entries[i].value);
  }
}
static void bitmap(const sn_bitmap* bitmap, size_t sheet) {
  if (bitmap) {
    text(bitmap->name);
    bytes(bitmap->bytes);
    if (bitmap->encoding == SN_ENCODING_RATTA_RLE) {
      const size_t widths[] = {1404, 1920}, heights[] = {1872, 2560};
      sn_raster* raster = nullptr;
      if (sn_bitmap_raster(bitmap, widths[sheet], heights[sheet], nullptr,
                           &raster, nullptr) == SN_OK) {
        bytes((sn_bytes){raster->gray, raster->width * raster->height});
        bytes((sn_bytes){raster->alpha, raster->width * raster->height});
        sn_raster_free(raster);
      }
    }
  }
}
static void fuzz_sink(sn_log_level level, const char* message, void* context) {
  (void)level;
  (void)context;
  text((sn_string){message, strlen(message)});
}
int LLVMFuzzerInitialize(int* argc, char*** argv);
int LLVMFuzzerInitialize(int* argc, char*** argv) {
  (void)argc;
  (void)argv;
  sn_log_install(fuzz_sink, nullptr);
  return 0;
}
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  sn_notebook* notebook = nullptr;
  sn_status status = sn_parse_notebook(data, size, &notebook, nullptr);
  if (status != SN_OK) return 0;
  /* Exercise both sheet sizes across inputs without decoding every bitmap
   * twice per execution. The last byte can vary independently of the header. */
  size_t sheet = size ? data[size - 1] & 1u : 0;
  text(notebook->file_id);
  text(notebook->equipment_raw);
  metadata(notebook->header);
  metadata(notebook->footer);
  bitmap(notebook->cover, sheet);
  for (size_t i = 0; i < notebook->template_count; i++) {
    text(notebook->templates[i].name);
    bitmap(&notebook->templates[i].bitmap, sheet);
  }
  for (size_t i = 0; i < notebook->title_count; i++) {
    bitmap(notebook->titles[i].bitmap, sheet);
    metadata(notebook->titles[i].metadata);
  }
  for (size_t i = 0; i < notebook->keyword_count; i++) {
    text(notebook->keywords[i].text);
    metadata(notebook->keywords[i].metadata);
  }
  for (size_t i = 0; i < notebook->link_count; i++) {
    const sn_link* l = notebook->links + i;
    text(l->text);
    text(l->target_path);
    text(l->target_file_id);
    text(l->target_page_id);
    text(l->timestamp);
    text(l->url);
    bitmap(l->bitmap, sheet);
    metadata(l->metadata);
  }
  for (size_t i = 0; i < notebook->highlight_count; i++) {
    const sn_highlight* h = notebook->highlights + i;
    text(h->comment);
    for (size_t j = 0; j < h->rect_count; j++) touched += h->rects[j].is_null;
    for (size_t j = 0; j < h->document_rect_count; j++)
      touched += h->document_rects[j].is_null;
  }
  for (size_t i = 0; i < notebook->page_count; i++) {
    const sn_page* page = notebook->pages + i;
    text(page->id);
    text(page->template_name);
    metadata(page->metadata);
    bytes(page->recognition_archive);
    for (size_t j = 0; j < page->layer_info_count; j++)
      text(page->layer_info[j].name);
    for (size_t j = 0; j < page->layer_count; j++) {
      bitmap(page->layers[j].bitmap, sheet);
      metadata(page->layers[j].metadata);
    }
    for (size_t j = 0; j < page->stroke_count; j++) {
      const sn_stroke* stroke = page->strokes + j;
      text(stroke->kind_name);
      for (size_t k = 0; k < stroke->point_count; k++)
        touched += stroke->points[k].pressure;
      for (size_t k = 0; k < stroke->pressure_count; k++)
        touched += stroke->pressures[k];
      for (size_t k = 0; k < stroke->contour_count; k++)
        for (size_t q = 0; q < stroke->contours[k].count; q++)
          touched += (uint64_t)isfinite(stroke->contours[k].points[q].x);
      for (size_t k = 0; k < stroke->attribute_count; k++)
        text(stroke->attributes[k]);
    }
    for (size_t j = 0; j < page->drawn_stroke_count; j++)
      touched += (uint32_t)page->drawn_strokes[j]->id;
    for (size_t j = 0; j < page->text_box_count; j++) {
      const sn_text_box* bitmap = page->text_boxes + j;
      text(bitmap->text);
      text(bitmap->font_path);
      text(bitmap->timestamp);
      for (size_t k = 0; k < bitmap->attribute_count; k++)
        text(bitmap->attributes[k]);
    }
    for (size_t j = 0; j < page->disabled_area_count; j++)
      touched += page->disabled_areas[j].is_null;
    if (page->recognition) {
      text(page->recognition->text);
      for (size_t j = 0; j < page->recognition->element_count; j++) {
        const sn_element* e = page->recognition->elements + j;
        text(e->kind);
        text(e->label);
        for (size_t k = 0; k < e->word_count; k++) text(e->words[k].label);
      }
    }
    sn_raster* raster = nullptr;
    status = sn_page_raster(page, nullptr, true, &raster, nullptr);
    if (status == SN_OK) {
      bytes((sn_bytes){raster->gray, raster->width * raster->height});
      bytes((sn_bytes){raster->alpha, raster->width * raster->height});
      sn_raster_free(raster);
    }
  }
  sn_notebook_free(notebook);
  return 0;
}
