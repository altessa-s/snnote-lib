/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */
#ifndef SN_WALK_H
#define SN_WALK_H
#include "sn_test.h"
#include "snnote/snnote.h"
[[maybe_unused]] static void walk_sn_string(const sn_string* value);
[[maybe_unused]] static void walk_sn_bytes(const sn_bytes* value);
[[maybe_unused]] static void walk_sn_point(const sn_point* value);
[[maybe_unused]] static void walk_sn_size(const sn_size* value);
[[maybe_unused]] static void walk_sn_rect(const sn_rect* value);
[[maybe_unused]] static void walk_sn_metadata_entry(
    const sn_metadata_entry* value);
[[maybe_unused]] static void walk_sn_metadata(const sn_metadata* value);
[[maybe_unused]] static void walk_sn_bitmap(const sn_bitmap* value);
[[maybe_unused]] static void walk_sn_ink(const sn_ink* value);
[[maybe_unused]] static void walk_sn_palette(const sn_palette* value);
[[maybe_unused]] static void walk_sn_layer_info(const sn_layer_info* value);
[[maybe_unused]] static void walk_sn_layer(const sn_layer* value);
[[maybe_unused]] static void walk_sn_stroke_point(const sn_stroke_point* value);
[[maybe_unused]] static void walk_sn_contour(const sn_contour* value);
[[maybe_unused]] static void walk_sn_stroke(const sn_stroke* value);
[[maybe_unused]] static void walk_sn_text_box(const sn_text_box* value);
[[maybe_unused]] static void walk_sn_word(const sn_word* value);
[[maybe_unused]] static void walk_sn_element(const sn_element* value);
[[maybe_unused]] static void walk_sn_recognition(const sn_recognition* value);
[[maybe_unused]] static void walk_sn_page(const sn_page* value);
[[maybe_unused]] static void walk_sn_title(const sn_title* value);
[[maybe_unused]] static void walk_sn_keyword(const sn_keyword* value);
[[maybe_unused]] static void walk_sn_link(const sn_link* value);
[[maybe_unused]] static void walk_sn_highlight(const sn_highlight* value);
[[maybe_unused]] static void walk_sn_template(const sn_template* value);
[[maybe_unused]] static void walk_sn_notebook(const sn_notebook* value);

static void walk_sn_string(const sn_string* value) {
  SN_CHECK(value != nullptr, "sn_string pointer");
  if (!value) return;
  SN_CHECK(value->data || !value->length, "string count and pointer");
  if (value->data)
    SN_CHECK(value->data[value->length] == 0, "string terminator at length");
}

static void walk_sn_bytes(const sn_bytes* value) {
  SN_CHECK(value != nullptr, "sn_bytes pointer");
  if (!value) return;
  SN_CHECK(value->data || !value->size, "byte count and pointer");
  for (size_t index = 0; index < value->size; index++) {
    volatile uint8_t byte = value->data[index];
    (void)byte;
  }
}

static void walk_sn_point(const sn_point* value) {
  SN_CHECK(value != nullptr, "sn_point pointer");
  if (!value) return;
  volatile double x = value->x;
  (void)x;
  volatile double y = value->y;
  (void)y;
}

static void walk_sn_size(const sn_size* value) {
  SN_CHECK(value != nullptr, "sn_size pointer");
  if (!value) return;
  volatile double width = value->width;
  (void)width;
  volatile double height = value->height;
  (void)height;
}

static void walk_sn_rect(const sn_rect* value) {
  SN_CHECK(value != nullptr, "sn_rect pointer");
  if (!value) return;
  volatile double x = value->x;
  (void)x;
  volatile double y = value->y;
  (void)y;
  volatile double width = value->width;
  (void)width;
  volatile double height = value->height;
  (void)height;
}

static void walk_sn_metadata_entry(const sn_metadata_entry* value) {
  SN_CHECK(value != nullptr, "sn_metadata_entry pointer");
  if (!value) return;
  walk_sn_string(&value->key);
  walk_sn_string(&value->value);
}

static void walk_sn_metadata(const sn_metadata* value) {
  SN_CHECK(value != nullptr, "sn_metadata pointer");
  if (!value) return;
  SN_CHECK(value->entries || !value->count,
           "sn_metadata.entries count and pointer");
  for (size_t index = 0; index < value->count; index++) {
    walk_sn_metadata_entry(value->entries + index);
  }
}

static void walk_sn_bitmap(const sn_bitmap* value) {
  SN_CHECK(value != nullptr, "sn_bitmap pointer");
  if (!value) return;
  SN_CHECK(value->encoding >= SN_ENCODING_RATTA_RLE &&
               value->encoding <= SN_ENCODING_OTHER,
           "sn_bitmap.encoding range");
  walk_sn_string(&value->name);
  walk_sn_bytes(&value->bytes);
}

static void walk_sn_ink(const sn_ink* value) {
  SN_CHECK(value != nullptr, "sn_ink pointer");
  if (!value) return;
  volatile uint8_t gray = value->gray;
  (void)gray;
  volatile uint8_t alpha = value->alpha;
  (void)alpha;
}

static void walk_sn_palette(const sn_palette* value) {
  SN_CHECK(value != nullptr, "sn_palette pointer");
  if (!value) return;
  for (size_t index = 0; index < 256; index++) walk_sn_ink(value->inks + index);
}

static void walk_sn_layer_info(const sn_layer_info* value) {
  SN_CHECK(value != nullptr, "sn_layer_info pointer");
  if (!value) return;
  walk_sn_string(&value->name);
}

static void walk_sn_layer(const sn_layer* value) {
  SN_CHECK(value != nullptr, "sn_layer pointer");
  if (!value) return;
  SN_CHECK(value->name >= SN_LAYER_MAIN && value->name <= SN_LAYER_BACKGROUND,
           "sn_layer.name range");
  if (value->bitmap) walk_sn_bitmap(value->bitmap);
  if (value->info) walk_sn_layer_info(value->info);
  walk_sn_metadata(&value->metadata);
}

static void walk_sn_stroke_point(const sn_stroke_point* value) {
  SN_CHECK(value != nullptr, "sn_stroke_point pointer");
  if (!value) return;
  walk_sn_point(&value->position);
}

static void walk_sn_contour(const sn_contour* value) {
  SN_CHECK(value != nullptr, "sn_contour pointer");
  if (!value) return;
  SN_CHECK(value->points || !value->count,
           "sn_contour.points count and pointer");
  for (size_t index = 0; index < value->count; index++) {
    walk_sn_point(value->points + index);
  }
}

static void walk_sn_stroke(const sn_stroke* value) {
  SN_CHECK(value != nullptr, "sn_stroke pointer");
  if (!value) return;
  walk_sn_string(&value->kind_name);
  SN_CHECK(value->kind >= SN_STROKE_KIND_FREEHAND &&
               value->kind <= SN_STROKE_KIND_OTHER,
           "sn_stroke.kind range");
  SN_CHECK(value->role >= SN_ROLE_INK && value->role <= SN_ROLE_OTHER,
           "sn_stroke.role range");
  SN_CHECK(value->status >= SN_STROKE_STATUS_LIVE &&
               value->status <= SN_STROKE_STATUS_OTHER,
           "sn_stroke.status range");
  SN_CHECK(value->category >= SN_CATEGORY_ORDINARY &&
               value->category <= SN_CATEGORY_OTHER,
           "sn_stroke.category range");
  SN_CHECK(value->points || !value->point_count,
           "sn_stroke.points count and pointer");
  for (size_t index = 0; index < value->point_count; index++) {
    walk_sn_stroke_point(value->points + index);
  }
  SN_CHECK(value->pressures || !value->pressure_count,
           "sn_stroke.pressures count and pointer");
  for (size_t index = 0; index < value->pressure_count; index++) {
    volatile uint16_t item = value->pressures[index];
    (void)item;
  }
  SN_CHECK(value->contours || !value->contour_count,
           "sn_stroke.contours count and pointer");
  for (size_t index = 0; index < value->contour_count; index++) {
    walk_sn_contour(value->contours + index);
  }
  walk_sn_rect(&value->bounds);
  SN_CHECK(value->attributes || !value->attribute_count,
           "sn_stroke.attributes count and pointer");
  for (size_t index = 0; index < value->attribute_count; index++) {
    walk_sn_string(value->attributes + index);
  }
}

static void walk_sn_text_box(const sn_text_box* value) {
  SN_CHECK(value != nullptr, "sn_text_box pointer");
  if (!value) return;
  walk_sn_rect(&value->rect);
  walk_sn_string(&value->text);
  walk_sn_string(&value->font_path);
  walk_sn_string(&value->timestamp);
  SN_CHECK(value->attributes || !value->attribute_count,
           "sn_text_box.attributes count and pointer");
  for (size_t index = 0; index < value->attribute_count; index++) {
    walk_sn_string(value->attributes + index);
  }
}

static void walk_sn_word(const sn_word* value) {
  SN_CHECK(value != nullptr, "sn_word pointer");
  if (!value) return;
  walk_sn_string(&value->label);
  walk_sn_rect(&value->box);
}

static void walk_sn_element(const sn_element* value) {
  SN_CHECK(value != nullptr, "sn_element pointer");
  if (!value) return;
  walk_sn_string(&value->kind);
  walk_sn_string(&value->label);
  SN_CHECK(value->words || !value->word_count,
           "sn_element.words count and pointer");
  for (size_t index = 0; index < value->word_count; index++) {
    walk_sn_word(value->words + index);
  }
}

static void walk_sn_recognition(const sn_recognition* value) {
  SN_CHECK(value != nullptr, "sn_recognition pointer");
  if (!value) return;
  SN_CHECK(value->elements || !value->element_count,
           "sn_recognition.elements count and pointer");
  for (size_t index = 0; index < value->element_count; index++) {
    walk_sn_element(value->elements + index);
  }
  walk_sn_string(&value->text);
}

static void walk_sn_page(const sn_page* value) {
  SN_CHECK(value != nullptr, "sn_page pointer");
  if (!value) return;
  walk_sn_string(&value->id);
  walk_sn_string(&value->template_name);
  SN_CHECK(value->orientation >= SN_ORIENTATION_PORTRAIT &&
               value->orientation <= SN_ORIENTATION_OTHER,
           "sn_page.orientation range");
  walk_sn_size(&value->size);
  SN_CHECK(value->layers || !value->layer_count,
           "sn_page.layers count and pointer");
  for (size_t index = 0; index < value->layer_count; index++) {
    walk_sn_layer(value->layers + index);
  }
  SN_CHECK(value->layer_info || !value->layer_info_count,
           "sn_page.layer_info count and pointer");
  for (size_t index = 0; index < value->layer_info_count; index++) {
    walk_sn_layer_info(value->layer_info + index);
  }
  SN_CHECK(value->strokes || !value->stroke_count,
           "sn_page.strokes count and pointer");
  for (size_t index = 0; index < value->stroke_count; index++) {
    walk_sn_stroke(value->strokes + index);
  }
  SN_CHECK(value->drawn_strokes || !value->drawn_stroke_count,
           "sn_page.drawn_strokes count and pointer");
  for (size_t index = 0; index < value->drawn_stroke_count; index++) {
    walk_sn_stroke(value->drawn_strokes[index]);
  }
  SN_CHECK(value->text_boxes || !value->text_box_count,
           "sn_page.text_boxes count and pointer");
  for (size_t index = 0; index < value->text_box_count; index++) {
    walk_sn_text_box(value->text_boxes + index);
  }
  if (value->recognition) walk_sn_recognition(value->recognition);
  walk_sn_bytes(&value->recognition_archive);
  SN_CHECK(value->disabled_areas || !value->disabled_area_count,
           "sn_page.disabled_areas count and pointer");
  for (size_t index = 0; index < value->disabled_area_count; index++) {
    walk_sn_rect(value->disabled_areas + index);
  }
  walk_sn_metadata(&value->metadata);
}

static void walk_sn_title(const sn_title* value) {
  SN_CHECK(value != nullptr, "sn_title pointer");
  if (!value) return;
  walk_sn_rect(&value->rect);
  walk_sn_rect(&value->original_rect);
  if (value->bitmap) walk_sn_bitmap(value->bitmap);
  walk_sn_metadata(&value->metadata);
}

static void walk_sn_keyword(const sn_keyword* value) {
  SN_CHECK(value != nullptr, "sn_keyword pointer");
  if (!value) return;
  walk_sn_rect(&value->rect);
  walk_sn_rect(&value->original_rect);
  walk_sn_string(&value->text);
  walk_sn_metadata(&value->metadata);
}

static void walk_sn_link(const sn_link* value) {
  SN_CHECK(value != nullptr, "sn_link pointer");
  if (!value) return;
  walk_sn_rect(&value->rect);
  walk_sn_string(&value->target_path);
  walk_sn_string(&value->target_file_id);
  walk_sn_string(&value->target_page_id);
  walk_sn_string(&value->timestamp);
  walk_sn_string(&value->text);
  walk_sn_string(&value->url);
  if (value->bitmap) walk_sn_bitmap(value->bitmap);
  walk_sn_metadata(&value->metadata);
}

static void walk_sn_highlight(const sn_highlight* value) {
  SN_CHECK(value != nullptr, "sn_highlight pointer");
  if (!value) return;
  walk_sn_string(&value->comment);
  SN_CHECK(value->rects || !value->rect_count,
           "sn_highlight.rects count and pointer");
  for (size_t index = 0; index < value->rect_count; index++) {
    walk_sn_rect(value->rects + index);
  }
  SN_CHECK(value->document_rects || !value->document_rect_count,
           "sn_highlight.document_rects count and pointer");
  for (size_t index = 0; index < value->document_rect_count; index++) {
    walk_sn_rect(value->document_rects + index);
  }
}

static void walk_sn_template(const sn_template* value) {
  SN_CHECK(value != nullptr, "sn_template pointer");
  if (!value) return;
  walk_sn_string(&value->name);
  walk_sn_bitmap(&value->bitmap);
}

static void walk_sn_notebook(const sn_notebook* value) {
  walk_sn_palette(&value->palette);
  SN_CHECK(value != nullptr, "sn_notebook pointer");
  if (!value) return;
  SN_CHECK(value->kind >= SN_KIND_NOTE && value->kind <= SN_KIND_MARK,
           "sn_notebook.kind range");
  SN_CHECK(value->equipment >= SN_EQUIPMENT_OTHER &&
               value->equipment <= SN_EQUIPMENT_MANTA,
           "sn_notebook.equipment range");
  walk_sn_string(&value->equipment_raw);
  walk_sn_string(&value->file_id);
  walk_sn_size(&value->sheet);
  SN_CHECK(value->pages || !value->page_count,
           "sn_notebook.pages count and pointer");
  for (size_t index = 0; index < value->page_count; index++) {
    walk_sn_page(value->pages + index);
  }
  SN_CHECK(value->titles || !value->title_count,
           "sn_notebook.titles count and pointer");
  for (size_t index = 0; index < value->title_count; index++) {
    walk_sn_title(value->titles + index);
  }
  SN_CHECK(value->keywords || !value->keyword_count,
           "sn_notebook.keywords count and pointer");
  for (size_t index = 0; index < value->keyword_count; index++) {
    walk_sn_keyword(value->keywords + index);
  }
  SN_CHECK(value->links || !value->link_count,
           "sn_notebook.links count and pointer");
  for (size_t index = 0; index < value->link_count; index++) {
    walk_sn_link(value->links + index);
  }
  SN_CHECK(value->highlights || !value->highlight_count,
           "sn_notebook.highlights count and pointer");
  for (size_t index = 0; index < value->highlight_count; index++) {
    walk_sn_highlight(value->highlights + index);
  }
  SN_CHECK(value->templates || !value->template_count,
           "sn_notebook.templates count and pointer");
  for (size_t index = 0; index < value->template_count; index++) {
    walk_sn_template(value->templates + index);
  }
  if (value->cover) walk_sn_bitmap(value->cover);
  walk_sn_metadata(&value->header);
  walk_sn_metadata(&value->footer);
}
#endif
