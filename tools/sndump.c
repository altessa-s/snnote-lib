/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include <inttypes.h>
#include <snnote/snnote.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sn_composition.h"
static void dump_rect(const sn_rect* value);
static void dump_entry(const sn_metadata_entry* value);
static bool json, first = true;
static void quote(const char* text, size_t count) {
  putchar('"');
  for (size_t index = 0; index < count; index++) {
    unsigned char ch = (unsigned char)text[index];
    if (ch == '"' || ch == '\\') {
      putchar('\\');
      putchar(ch);
    } else if (ch < 32)
      printf("\\u%04x", ch);
    else
      putchar(ch);
  }
  putchar('"');
}
static void key(const char* text) {
  if (json) {
    if (!first) putchar(',');
    putchar('[');
    quote(text, strlen(text));
    putchar(',');
  } else {
    fputs(text, stdout);
    putchar('\t');
  }
  first = false;
}
static void end(void) {
  if (json)
    putchar(']');
  else
    putchar('\n');
}
static void integer(const char* field, int64_t value) {
  key(field);
  printf("%" PRId64, value);
  end();
}
static void number(const char* field, double value) {
  key(field);
  printf("%.17e", value);
  end();
}
static void boolean(const char* field, bool value) {
  key(field);
  fputs(value ? "true" : "false", stdout);
  end();
}
static void string(const char* field, sn_string value) {
  key(field);
  if (value.data)
    quote(value.data, value.length);
  else
    fputs("null", stdout);
  end();
}
static uint64_t digest(const uint8_t* input, size_t count) {
  uint64_t hash = UINT64_C(14695981039346656037);
  for (size_t index = 0; index < count; index++)
    hash = (hash ^ input[index]) * UINT64_C(1099511628211);
  return hash;
}
static void bytes(const char* field, sn_bytes payload) {
  key(field);
  if (!payload.data)
    fputs("null", stdout);
  else {
    putchar('"');
    for (size_t index = 0; index < payload.size; index++)
      printf("%02x", payload.data[index]);
    putchar('"');
  }
  end();
}
static void dump_text_box(const sn_text_box* value) {
  integer("id", (int64_t)(value->id));
  dump_rect(&value->rect);
  string("text", value->text);
  boolean("has_font", value->has_font_size);
  number("font", value->has_font_size ? value->font_size : 0);
  string("font_path", value->font_path);
  string("timestamp", value->timestamp);
  integer("attributes", (int64_t)(value->attribute_count));
  for (size_t index = 0; index < value->attribute_count; index++)
    string("item", (value->attributes)[index]);
}
static void dump_metadata(const sn_metadata* value) {
  integer("entries", (int64_t)(value->count));
  for (size_t index = 0; index < value->count; index++)
    dump_entry(&(value->entries)[index]);
}
static void dump_entry(const sn_metadata_entry* value) {
  string("key", value->key);
  string("value", value->value);
}
static void dump_point(const sn_point* value) {
  number("x", value->x);
  number("y", value->y);
}
static void dump_rect(const sn_rect* value) {
  boolean("null", value->is_null);
  number("x", value->is_null ? 0 : value->x);
  number("y", value->is_null ? 0 : value->y);
  number("width", value->is_null ? 0 : value->width);
  number("height", value->is_null ? 0 : value->height);
}
static void dump_bitmap(const sn_bitmap* value) {
  integer("encoding", (int64_t)(value->encoding));
  string("name", value->name);
  bytes("bytes", value->bytes);
}
static void dump_layer_info(const sn_layer_info* value) {
  integer("id", (int64_t)(value->layer_id));
  string("name", value->name);
  boolean("background", value->is_background);
  boolean("visible", value->is_visible);
  boolean("deleted", value->is_deleted);
  boolean("current", value->is_current);
}
static void dump_layer(const sn_layer* value) {
  integer("name", (int64_t)(value->name));
  boolean("bitmap", value->bitmap != NULL);
  if (value->bitmap) dump_bitmap(value->bitmap);
  boolean("info", value->info != NULL);
  if (value->info) dump_layer_info(value->info);
  dump_metadata(&value->metadata);
  boolean("stacked", value->is_stacked);
  boolean("visible", value->is_visible);
}
static void dump_stroke_point(const sn_stroke_point* value) {
  dump_point(&value->position);
  integer("pressure", (int64_t)(value->pressure));
}
static void dump_contour(const sn_contour* value) {
  integer("points", (int64_t)(value->count));
  for (size_t index = 0; index < value->count; index++)
    dump_point(&(value->points)[index]);
}
static void dump_stroke(const sn_stroke* value) {
  integer("raw_role", (int64_t)(value->role_raw));
  integer("id", (int64_t)(value->id));
  integer("pen", (int64_t)(value->pen));
  integer("color", (int64_t)(value->color));
  integer("thickness", (int64_t)(value->thickness));
  string("kind", value->kind_name);
  integer("role", (int64_t)(value->role));
  integer("status", (int64_t)(value->status_raw));
  integer("operation", (int64_t)(value->operation));
  integer("layer", (int64_t)(value->layer));
  integer("page", (int64_t)(value->page_number));
  boolean("drawn", value->is_drawn);
  boolean("has_style", value->has_style);
  integer("category", (int64_t)(value->category));
  integer("style", (int64_t)(value->has_style ? value->style : 0));
  integer("points", (int64_t)(value->point_count));
  for (size_t index = 0; index < value->point_count; index++)
    dump_stroke_point(&(value->points)[index]);
  integer("contours", (int64_t)(value->contour_count));
  for (size_t index = 0; index < value->contour_count; index++)
    dump_contour(&(value->contours)[index]);
  dump_rect(&value->bounds);
  integer("attributes", (int64_t)(value->attribute_count));
  for (size_t index = 0; index < value->attribute_count; index++)
    string("item", (value->attributes)[index]);
  integer("pressures", (int64_t)value->pressure_count);
  for (size_t index = 0; index < value->pressure_count; index++)
    integer("pressure", value->pressures[index]);
}
static void dump_word(const sn_word* value) {
  string("label", value->label);
  dump_rect(&value->box);
}
static void dump_element(const sn_element* value) {
  string("kind", value->kind);
  string("label", value->label);
  integer("words", (int64_t)(value->word_count));
  for (size_t index = 0; index < value->word_count; index++)
    dump_word(&(value->words)[index]);
}
static void dump_recognition(const sn_recognition* value) {
  string("text", value->text);
  integer("elements", (int64_t)(value->element_count));
  for (size_t index = 0; index < value->element_count; index++)
    dump_element(&(value->elements)[index]);
}
static int compare_templates(const void* left, const void* right) {
  const sn_template* const* first = left;
  const sn_template* const* second = right;
  sn_string a = (*first)->name, b = (*second)->name;
  size_t length = a.length < b.length ? a.length : b.length;
  int order = length ? memcmp(a.data, b.data, length) : 0;
  return order ? order : (a.length > b.length) - (a.length < b.length);
}

static void dump_page(const sn_page* value, const sn_palette* palette) {
  integer("raw_orientation", (int64_t)(value->orientation_raw));
  integer("number", (int64_t)(value->number));
  string("id", value->id);
  string("template", value->template_name);
  number("width", value->size.width);
  number("height", value->size.height);
  integer("layers", (int64_t)(value->layer_count));
  for (size_t index = 0; index < value->layer_count; index++)
    dump_layer(&(value->layers)[index]);
  integer("layer_info", (int64_t)(value->layer_info_count));
  for (size_t index = 0; index < value->layer_info_count; index++)
    dump_layer_info(&(value->layer_info)[index]);
  integer("strokes", (int64_t)(value->stroke_count));
  for (size_t index = 0; index < value->stroke_count; index++)
    dump_stroke(&(value->strokes)[index]);
  integer("skipped_strokes", (int64_t)(value->skipped_strokes));
  boolean("recognition", value->recognition != NULL);
  if (value->recognition) dump_recognition(value->recognition);
  bytes("archive", value->recognition_archive);
  integer("disabled", (int64_t)(value->disabled_area_count));
  for (size_t index = 0; index < value->disabled_area_count; index++)
    dump_rect(&(value->disabled_areas)[index]);
  dump_metadata(&value->metadata);
  integer("text_boxes", (int64_t)(value->text_box_count));
  for (size_t index = 0; index < value->text_box_count; index++)
    dump_text_box(&(value->text_boxes)[index]);
  integer("drawn_strokes", (int64_t)value->drawn_stroke_count);
  for (size_t index = 0; index < value->drawn_stroke_count; index++)
    integer("drawn_id", value->drawn_strokes[index]->id);
  for (size_t index = 0; index < 256; index++) {
    integer("palette_gray", palette->inks[index].gray);
    integer("palette_alpha", palette->inks[index].alpha);
  }
  integer("orientation", value->orientation);
  sn_raster* raster = NULL;
  sn_status status = sn_page_raster(value, palette, true, &raster, NULL);
  integer("raster_status", status);
  if (raster) {
    char hash[17];
    snprintf(hash, sizeof hash, "%016" PRIx64,
             digest(raster->gray, raster->width * raster->height));
    string("gray", (sn_string){hash, 16});
    snprintf(hash, sizeof hash, "%016" PRIx64,
             digest(raster->alpha, raster->width * raster->height));
    string("alpha", (sn_string){hash, 16});
    sn_raster_free(raster);
  }
}
static void dump_title(const sn_title* value) {
  integer("page", (int64_t)(value->page_number));
  integer("level", (int64_t)(value->level));
  integer("style", (int64_t)(value->style));
  dump_rect(&value->rect);
  dump_rect(&value->original_rect);
  boolean("bitmap", value->bitmap != NULL);
  if (value->bitmap) dump_bitmap(value->bitmap);
  dump_metadata(&value->metadata);
}
static void dump_keyword(const sn_keyword* value) {
  integer("page", (int64_t)(value->page_number));
  string("text", value->text);
  dump_rect(&value->rect);
  dump_rect(&value->original_rect);
  dump_metadata(&value->metadata);
}
static void dump_link(const sn_link* value) {
  integer("page", (int64_t)(value->page_number));
  integer("kind", (int64_t)(value->kind));
  integer("direction", (int64_t)(value->direction));
  integer("style", (int64_t)(value->style));
  dump_rect(&value->rect);
  string("path", value->target_path);
  string("file_id", value->target_file_id);
  string("page_id", value->target_page_id);
  boolean("has_page", value->has_target_page_number);
  integer(
      "target_page",
      (int64_t)(value->has_target_page_number ? value->target_page_number : 0));
  string("timestamp", value->timestamp);
  string("text", value->text);
  boolean("bitmap", value->bitmap != NULL);
  if (value->bitmap) dump_bitmap(value->bitmap);
  dump_metadata(&value->metadata);
}
static void dump_highlight(const sn_highlight* value) {
  integer("page", (int64_t)(value->page_number));
  integer("kind", (int64_t)(value->kind));
  integer("color", (int64_t)(value->color_type));
  string("comment", value->comment);
  boolean("has_date", value->has_date);
  number("date", value->has_date ? value->date : 0);
  integer("rects", (int64_t)(value->rect_count));
  for (size_t index = 0; index < value->rect_count; index++)
    dump_rect(&(value->rects)[index]);
  integer("document_rects", (int64_t)(value->document_rect_count));
  for (size_t index = 0; index < value->document_rect_count; index++)
    dump_rect(&(value->document_rects)[index]);
}
static void dump_notebook(const sn_notebook* value) {
  integer("kind", (int64_t)(value->kind));
  integer("version", (int64_t)(value->format_version));
  string("equipment", value->equipment_raw);
  string("file_id", value->file_id);
  number("width", value->sheet.width);
  number("height", value->sheet.height);
  integer("skipped_pages", (int64_t)(value->skipped_pages));
  dump_metadata(&value->header);
  dump_metadata(&value->footer);
  integer("pages", (int64_t)(value->page_count));
  for (size_t index = 0; index < value->page_count; index++)
    dump_page(&(value->pages)[index], &value->palette);
  integer("titles", (int64_t)(value->title_count));
  for (size_t index = 0; index < value->title_count; index++)
    dump_title(&(value->titles)[index]);
  integer("keywords", (int64_t)(value->keyword_count));
  for (size_t index = 0; index < value->keyword_count; index++)
    dump_keyword(&(value->keywords)[index]);
  integer("links", (int64_t)(value->link_count));
  for (size_t index = 0; index < value->link_count; index++)
    dump_link(&(value->links)[index]);
  integer("highlights", (int64_t)(value->highlight_count));
  for (size_t index = 0; index < value->highlight_count; index++)
    dump_highlight(&(value->highlights)[index]);
  boolean("cover", value->cover != NULL);
  if (value->cover) dump_bitmap(value->cover);
  integer("cover_type", (int64_t)(value->cover_type));
  integer("templates", (int64_t)value->template_count);
  const sn_template** order = malloc(value->template_count * sizeof *order);
  if (!order && value->template_count) exit(2);
  for (size_t index = 0; index < value->template_count; index++)
    order[index] = &value->templates[index];
  qsort(order, value->template_count, sizeof *order, compare_templates);
  for (size_t index = 0; index < value->template_count; index++) {
    const sn_template* t = order[index];
    string("name", t->name);
    dump_bitmap(&t->bitmap);
  }
  free(order);
}
int main(int argc, char** argv) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif

  const char* path = NULL;
  for (int index = 1; index < argc; index++) {
    if (!strcmp(argv[index], "--json"))
      json = true;
    else
      path = argv[index];
  }
  if (!path) {
    fputs("usage: sndump [--json] notebook.note\n", stderr);
    return 2;
  }
  FILE* f = fopen(path, "rb");
  if (!f) return 2;
  if (fseek(f, 0, SEEK_END)) {
    fclose(f);
    return 2;
  }
  long length = ftell(f);
  if (length < 0) {
    fclose(f);
    return 2;
  }
  rewind(f);
  uint8_t* data = malloc((size_t)length + 1);
  if (!data) {
    fclose(f);
    return 2;
  }
  size_t size = fread(data, 1, (size_t)length, f);
  fclose(f);
  if (size != (size_t)length) {
    free(data);
    return 2;
  }
  sn_notebook* count = NULL;
  sn_error error;
  sn_status status = sn_parse_notebook(data, size, &count, &error);
  free(data);
  if (status != SN_OK) {
    fprintf(stderr, "%s\n", error.message);
    return 1;
  }
  if (json) putchar('[');
  dump_notebook(count);
  if (json) puts("]");
  sn_notebook_free(count);
  return fflush(stdout) != 0 || ferror(stdout) ? 2 : 0;
}
