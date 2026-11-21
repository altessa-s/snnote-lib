/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_composition.h"
#include "sn_internal.h"
#include "sn_test.h"
static const struct {
  const char* name;
  size_t pages, titles, keywords, links, highlights;
} cases[] = {
    {"samples/epub-n6-20260016.mark", 1, 0, 0, 0, 0},
    {"samples/layers-dense-n6-20260016.note", 1, 0, 0, 0, 0},
    {"samples/pdf-highlights-n6-20260016.mark", 1, 0, 0, 0, 3},
    {"samples/picture-links-cover-n6-20260016.note", 2, 0, 1, 2, 0},
    {"samples/textbox-links-n6-20260016.note", 2, 1, 0, 2, 0},
    {"supernote-typescript/blank-a6x-3.15.27-shapes-rtr.note", 1, 4, 1, 0, 0},
    {"supernote-typescript/blank-a6x-3.15.27-two-pages.note", 2, 0, 0, 1, 0},
    {"supernote-typescript/blank-a6x-3.26.40-two-pages.note", 2, 0, 0, 0, 0},
    {"supernote-typescript/blank-n5-20230015-manta.note", 1, 0, 0, 0, 0},
    {"supernote-typescript/caligraphy-n5-20260016-widths-erase.note", 4, 0, 0,
     0, 0},
    {"supernote-typescript/color-n6-20230015-unknown-palette.note", 1, 0, 0, 0,
     0},
    {"supernote-typescript/demo-a5x-20230015-1to10.note", 10, 0, 0, 0, 0},
    {"supernote-typescript/digest-n5-20230015-test.mark", 1, 0, 0, 0, 0},
    {"supernote-typescript/erase-n5-20260016-all-mechanisms.note", 1, 0, 0, 0,
     0},
    {"supernote-typescript/erase-n5-20260016-mixed-colors.note", 2, 0, 0, 0, 0},
    {"supernote-typescript/erase-n5-20260016-no-white-pen.note", 1, 0, 0, 0, 0},
    {"supernote-typescript/erase-n5-20260016-white-pen-cover.note", 4, 0, 0, 0,
     0},
    {"supernote-typescript/erase-n6-20230015-horizontal-1270.note", 1, 0, 0, 0,
     0},
    {"supernote-typescript/heading-n5-20260016-backgrounds-marker.note", 3, 4,
     0, 0, 0},
    {"supernote-typescript/ink-a5x-2.14.28-old-pen-width.note", 1, 0, 0, 0, 0},
    {"supernote-typescript/layout-n5-20230015-horizontal-1090.note", 1, 0, 0, 0,
     0},
    {"supernote-typescript/layout-n5-20230015-vertical-1000.note", 1, 0, 0, 0,
     0},
    {"supernote-typescript/layout-n6-20230015-vertical-1180.note", 1, 0, 0, 0,
     0},
    {"supernote-typescript/line-n5-20260016-ruler-tool.note", 3, 0, 0, 0, 0},
    {"supernote-typescript/link-n6-3.26.40-partial-erase-3p.note", 3, 0, 3, 3,
     0},
    {"supernote-typescript/render-n6-20230015-moonchild-user-bg.note", 1, 0, 0,
     0, 0},
    {"supernote-typescript/rtr-n5-20230015-recognition.note", 1, 0, 0, 0, 0},
    {"supernote-typescript/sticker-n5-20260016-plugin-artwork.note", 3, 0, 0, 0,
     0},
    {"supernote-typescript/stroke-n5-20260016-20260809.note", 5, 0, 0, 0, 0},
    {"supernote-typescript/"
     "stroke-n5-20260016-isolation-tools-colors-widths.note",
     5, 0, 0, 0, 0},
    {"supernote-typescript/test-a5x-20220011-old-pen-ids.note", 2, 2, 2, 0, 0},
    {"supernote-typescript/turkish-a6x-20230015-handwriting-erase.note", 1, 0,
     0, 0, 0}};
int main(void) {
  for (size_t fixture = 0; fixture < sizeof cases / sizeof *cases; fixture++) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", SN_FIXTURE_DIR, cases[fixture].name);
    size_t size = 0;
    uint8_t* data = sn_test_read_file(path, &size);
    SN_CHECK(data != nullptr, "fixture %s", cases[fixture].name);
    if (!data) continue;
    sn_notebook* notebook = nullptr;
    sn_error e;
    sn_test_work_reset();
    sn_status status = sn_parse_notebook(data, size, &notebook, &e);
    SN_CHECK(sn_test_work_used() <= sn_test_work_limit() / 8,
             "fixture work margin %s", cases[fixture].name);
    SN_CHECK(sn_test_work_used() <= size * SN_PARSE_WORK_MULTIPLE / 8,
             "fixture ratio calibration");
    printf("fixture %s input %zu peak %zu work %zu ceiling %zu\n",
           cases[fixture].name, size, sn_test_arena_peak(), sn_test_work_used(),
           sn_test_work_limit());
    SN_CHECK(sn_test_decodes() > 0 && !sn_test_duplicate_decodes(),
             "each block interpretation decoded once");
    size_t steps = sn_test_cache_probes(),
           comparisons = sn_test_metadata_comparisons;
    size_t decodes = sn_test_decodes();
    void* padding[8];
    for (size_t index = 0; index < 8; index++)
      padding[index] = malloc(123 + index * 65537);
    for (size_t index = 0; index < 8; index += 2) {
      free(padding[index]);
      padding[index] = nullptr;
    }
    sn_test_work_reset();
    sn_notebook* second = nullptr;
    sn_status repeated = sn_parse_notebook(data, size, &second, nullptr);
    SN_CHECK(repeated == status && steps == sn_test_cache_probes() &&
                 comparisons == sn_test_metadata_comparisons &&
                 decodes == sn_test_decodes() && !sn_test_duplicate_decodes(),
             "fixture heap-layout determinism");
    sn_notebook_free(second);
    for (size_t index = 0; index < 8; index++) free(padding[index]);
    free(data);
    SN_CHECK(status == SN_OK, "%s: %s", cases[fixture].name, e.message);
    if (!notebook) continue;
    SN_CHECK(notebook->page_count == cases[fixture].pages &&
                 !notebook->skipped_pages,
             "page count %s", cases[fixture].name);
    SN_CHECK(notebook->title_count == cases[fixture].titles &&
                 notebook->keyword_count == cases[fixture].keywords &&
                 notebook->link_count == cases[fixture].links &&
                 notebook->highlight_count == cases[fixture].highlights,
             "annotations %s", cases[fixture].name);
    for (size_t i = 0; i < notebook->page_count; i++) {
      const sn_page* page = notebook->pages + i;
      SN_CHECK(!page->skipped_strokes, "whole stroke log %s",
               cases[fixture].name);
      for (size_t j = 0; j < page->stroke_count; j++) {
        const sn_stroke* stroke = page->strokes + j;
        SN_CHECK(stroke->has_style, "tail %s", cases[fixture].name);
        for (size_t k = 0; k < stroke->point_count; k++)
          SN_CHECK(isfinite(stroke->points[k].position.x) &&
                       isfinite(stroke->points[k].position.y),
                   "finite centreline");
      }
      sn_raster* raster = nullptr;
      status = sn_page_raster(page, nullptr, true, &raster, nullptr);
      SN_CHECK(status == SN_OK || status == SN_ERR_IMAGE_ENCODING,
               "raster %s page %lld", cases[fixture].name,
               (long long)page->number);
      if (raster) {
        SN_CHECK(raster->width == (size_t)page->size.width &&
                     raster->height == (size_t)page->size.height,
                 "raster sheet");
        sn_raster_free(raster);
      }
      for (size_t j = 0; j < page->layer_count; j++)
        if (page->layers[j].bitmap &&
            page->layers[j].bitmap->encoding == SN_ENCODING_RATTA_RLE) {
          uint8_t* codes = nullptr;
          size_t code_count;
          status = sn_bitmap_codes(
              page->layers[j].bitmap, (size_t)page->size.width,
              (size_t)page->size.height, &codes, &code_count, nullptr);
          SN_CHECK(status == SN_OK, "layer decodes %s", cases[fixture].name);
          sn_codes_free(codes);
        }
    }
    sn_notebook_free(notebook);
  }
  return sn_test_report("corpus");
}
