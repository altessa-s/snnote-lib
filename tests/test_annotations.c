/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_internal.h"
#include "sn_test.h"
static sn_notebook* read_note(const char* name) {
  char path[1024];
  snprintf(path, sizeof path, "%s/%s", SN_FIXTURE_DIR, name);
  size_t size = 0;
  uint8_t* bytes = sn_test_read_file(path, &size);
  if (!bytes) {
    SN_CHECK(false, "fixture");
    return nullptr;
  }
  sn_notebook* notebook = nullptr;
  sn_status status = sn_parse_notebook(bytes, size, &notebook, nullptr);
  free(bytes);
  SN_CHECK(status == SN_OK, "notebook");
  return notebook;
}
static void samples(void) {
  sn_notebook* notebook = read_note("samples/textbox-links-n6-20260016.note");
  if (!notebook) return;
  const sn_page* page = notebook->pages;
  SN_CHECK(page->text_box_count == 3, "three text boxes");
  const char* text[] = {"Проверка текстового блока",
                        "Проверка текстового блока 2, другой размер",
                        "Еще один текстовый блок"};
  double fonts[] = {48, 68, 40};
  for (size_t i = 0; i < page->text_box_count && i < 3; i++) {
    SN_CHECK(sn_equal(page->text_boxes[i].text, text[i]) &&
                 page->text_boxes[i].has_font_size &&
                 page->text_boxes[i].font_size == fonts[i],
             "text and font");
    SN_CHECK(
        page->disabled_areas[i].x == page->text_boxes[i].rect.x &&
            page->disabled_areas[i].width == page->text_boxes[i].rect.width,
        "disabled area");
  }
  SN_CHECK(notebook->links[0].target_page_number == 2 &&
               notebook->links[1].kind == 4 &&
               sn_equal(notebook->links[1].url, "https://example.com"),
           "page and web links");
  SN_CHECK(notebook->titles[0].style == 1000254 &&
               sn_page_layer(page, SN_LAYER_BACKGROUND)->bitmap->encoding ==
                   SN_ENCODING_PNG,
           "heading and imported template");
  sn_notebook_free(notebook);
  notebook = read_note("samples/layers-dense-n6-20260016.note");
  if (!notebook) return;
  page = notebook->pages;
  const sn_layer* hidden = sn_page_layer(page, SN_LAYER_2);
  SN_CHECK(
      hidden && !hidden->is_stacked && !hidden->is_visible && hidden->bitmap,
      "hidden layer retained");
  SN_CHECK(page->drawn_stroke_count == 48, "visible strokes");
  sn_notebook_free(notebook);
  notebook = read_note("samples/pdf-highlights-n6-20260016.mark");
  if (!notebook) return;
  SN_CHECK(notebook->kind == SN_KIND_MARK && notebook->highlight_count == 3,
           "document highlights");
  SN_CHECK(sn_equal(notebook->highlights[1].comment, "Test comment") &&
               notebook->highlights[2].kind == 1,
           "comment and underline");
  SN_CHECK(notebook->highlights[0].rects[0].x == 114 &&
               notebook->highlights[0].rects[0].width == 1159 &&
               notebook->highlights[0].document_rects[0].width == 505,
           "coordinate systems");
  sn_notebook_free(notebook);
  notebook =
      read_note("supernote-typescript/blank-a6x-3.15.27-shapes-rtr.note");
  if (!notebook) return;
  const sn_recognition* recognition = notebook->pages[0].recognition;
  SN_CHECK(recognition &&
               sn_equal(recognition->text, "Square\nTriangle\nCircle\nStar"),
           "recognized text");
  SN_CHECK(recognition && recognition->elements[0].words[1].box.is_null,
           "word separator");
  sn_notebook_free(notebook);
  notebook = read_note(
      "supernote-typescript/heading-n5-20260016-backgrounds-marker.note");
  if (!notebook) return;
  SN_CHECK(notebook->pages[1].recognition &&
               !notebook->pages[1].recognition->element_count &&
               !notebook->pages[2].recognition,
           "empty is not absent");
  int64_t styles[] = {1000254, 1157254, 1201000, 1000000};
  for (size_t i = 0; i < 4; i++)
    SN_CHECK(notebook->titles[i].style == styles[i], "heading style");
  sn_notebook_free(notebook);
}
#include "sn_encoded.h"
static void strict_fields(void) {
  sn_arena arena = {0};
  const sn_recognition* recognition =
      sn_recognition_read(&arena, encoded(&arena, "{}"));
  SN_CHECK(recognition && !recognition->element_count,
           "missing optional elements");
  recognition = sn_recognition_read(
      &arena, encoded(&arena, "{\"elements\":[{\"type\":4}]}"));
  SN_CHECK(!recognition, "wrong optional type fails whole recognition");
  recognition = sn_recognition_read(
      &arena,
      encoded(&arena,
              "{\"elements\":[{\"words\":[{\"bounding-box\":{\"x\":1}}]}]}"));
  SN_CHECK(!recognition, "required box fields");
  sn_notebook notebook = {0};
  sn_highlights(
      &arena,
      encoded(&arena,
              "{\"2\":[{\"annotationType\":1,\"time\":1000}],\"0\":[{}]}"),
      &notebook);
  SN_CHECK(notebook.highlight_count == 2 &&
               notebook.highlights[0].page_number == 1 &&
               notebook.highlights[1].page_number == 3 &&
               notebook.highlights[1].date == 1,
           "numeric page sorting and seconds");
  notebook = (sn_notebook){0};
  sn_highlights(&arena, encoded(&arena, "{\"0\":[{\"time\":\"1000\"}]}"),
                &notebook);
  SN_CHECK(!notebook.highlight_count, "numeric strings rejected");
  sn_arena_free(&arena);
}
static void finite_attributes(void) {
  const char* invalid[] = {"1e400", "-1e400", "1.7976931348623159e308"};
  for (size_t i = 0; i < 3; i++) {
    char text[128];
    for (size_t component = 0; component < 4; component++) {
      snprintf(
          text, sizeof text, "%s,%s,%s,%s", component == 0 ? invalid[i] : "1",
          component == 1 ? invalid[i] : "2", component == 2 ? invalid[i] : "3",
          component == 3 ? invalid[i] : "4");
      sn_rect rect = {.x = 42, .is_null = true};
      SN_CHECK(!sn_rect_parse((sn_string){text, strlen(text)}, &rect) &&
                   rect.x == 42 && rect.is_null,
               "overflow leaves rectangle fallback unchanged");
    }
    sn_string attributes[11] = {0};
    attributes[10] = (sn_string){invalid[i], strlen(invalid[i])};
    sn_stroke stroke = {.category = SN_CATEGORY_TEXT_BOX,
                        .attributes = attributes,
                        .attribute_count = 11};
    sn_page page = {.strokes = &stroke, .stroke_count = 1};
    sn_arena arena = {0};
    sn_derived(&arena, &page);
    SN_CHECK(page.text_box_count == 1 && !page.text_boxes[0].has_font_size &&
                 isfinite(page.text_boxes[0].font_size),
             "overflowing font size is absent");
    sn_arena_free(&arena);
  }
}

int main(void) {
  finite_attributes();
  samples();
  strict_fields();
  return sn_test_report("annotations");
}
