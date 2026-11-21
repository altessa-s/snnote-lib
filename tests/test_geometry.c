/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_composition.h"
#include "sn_internal.h"
#include "sn_test.h"
static sn_notebook* load(const char* name) {
  char path[1024];
  snprintf(path, sizeof path, "%s/supernote-typescript/%s", SN_FIXTURE_DIR,
           name);
  size_t size = 0;
  uint8_t* data = sn_test_read_file(path, &size);
  if (!data) {
    SN_CHECK(false, "fixture");
    return nullptr;
  }
  sn_notebook* notebook = nullptr;
  sn_status status = sn_parse_notebook(data, size, &notebook, nullptr);
  free(data);
  SN_CHECK(status == SN_OK, "parse");
  return notebook;
}
static bool ink_near(sn_point point, const sn_raster* raster) {
  long x = lround(point.x), y = lround(point.y);
  for (long dy = -2; dy <= 2; dy++)
    for (long dx = -2; dx <= 2; dx++) {
      long px = x + dx, py = y + dy;
      if (px >= 0 && py >= 0 && (size_t)px < raster->width &&
          (size_t)py < raster->height &&
          raster->gray[(size_t)py * raster->width + (size_t)px] < 250)
        return true;
    }
  return false;
}
static void ink(void) {
  const char* names[] = {"blank-n5-20230015-manta.note",
                         "erase-n6-20230015-horizontal-1270.note",
                         "layout-n5-20230015-horizontal-1090.note",
                         "ink-a5x-2.14.28-old-pen-width.note",
                         "test-a5x-20220011-old-pen-ids.note",
                         "turkish-a6x-20230015-handwriting-erase.note"};
  for (size_t i = 0; i < 6; i++) {
    sn_notebook* notebook = load(names[i]);
    if (!notebook) continue;
    const sn_page* page = notebook->pages;
    sn_raster* raster = nullptr;
    sn_status status = sn_page_raster(page, nullptr, false, &raster, nullptr);
    SN_CHECK(status == SN_OK, "handwriting raster");
    if (!raster) {
      sn_notebook_free(notebook);
      continue;
    }
    size_t total = 0, on = 0;
    for (size_t stroke_index = 0; stroke_index < page->drawn_stroke_count;
         stroke_index++) {
      const sn_stroke* stroke = page->drawn_strokes[stroke_index];
      if (stroke->role != SN_ROLE_INK || stroke->color >= 250) continue;
      for (size_t point_index = 0; point_index < stroke->point_count;
           point_index++) {
        total++;
        if (ink_near(stroke->points[point_index].position, raster)) on++;
      }
    }
    SN_CHECK(total > 0 && (double)on / (double)total > 0.98,
             "coordinates on ink: %s %zu/%zu", names[i], on, total);
    sn_raster_free(raster);
    sn_notebook_free(notebook);
  }
}
static void bounds(void) {
  sn_notebook* notebook = load("turkish-a6x-20230015-handwriting-erase.note");
  if (!notebook) return;
  for (size_t i = 0; i < notebook->pages[0].drawn_stroke_count; i++) {
    const sn_stroke* stroke = notebook->pages[0].drawn_strokes[i];
    if (stroke->point_count < 2) continue;
    for (size_t stroke_index = 0; stroke_index < stroke->point_count;
         stroke_index++) {
      sn_point page = stroke->points[stroke_index].position;
      SN_CHECK(page.x >= stroke->bounds.x - 2 &&
                   page.x <= stroke->bounds.x + stroke->bounds.width + 2 &&
                   page.y >= stroke->bounds.y - 2 &&
                   page.y <= stroke->bounds.y + stroke->bounds.height + 2,
               "declared bounds");
    }
  }
  sn_notebook_free(notebook);
  notebook = load("blank-n5-20230015-manta.note");
  if (!notebook) return;
  for (size_t i = 0; i < notebook->pages[0].drawn_stroke_count; i++) {
    const sn_stroke* stroke = notebook->pages[0].drawn_strokes[i];
    double minx = INFINITY, miny = INFINITY, maxx = -INFINITY, maxy = -INFINITY;
    for (size_t stroke_index = 0; stroke_index < stroke->contour_count;
         stroke_index++)
      for (size_t point_index = 0;
           point_index < stroke->contours[stroke_index].count; point_index++) {
        sn_point page = stroke->contours[stroke_index].points[point_index];
        minx = fmin(minx, page.x);
        miny = fmin(miny, page.y);
        maxx = fmax(maxx, page.x);
        maxy = fmax(maxy, page.y);
      }
    if (!isfinite(minx) || stroke->point_count < 2) continue;
    double slack = (double)stroke->thickness / 100 + 2;
    for (size_t stroke_index = 0; stroke_index < stroke->point_count;
         stroke_index++) {
      sn_point page = stroke->points[stroke_index].position;
      SN_CHECK(page.x >= minx - slack && page.x <= maxx + slack &&
                   page.y >= miny - slack && page.y <= maxy + slack,
               "outline encloses centreline");
    }
  }
  sn_notebook_free(notebook);
}
int main(void) {
  ink();
  bounds();
  return sn_test_report("geometry");
}
