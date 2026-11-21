/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_generate.h"
#include "sn_internal.h"
#include "sn_test.h"
#include "sn_walk.h"

static uint64_t next(uint64_t* state) {
  *state += UINT64_C(0x9e3779b97f4a7c15);
  uint64_t z = *state;
  z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
  return z ^ (z >> 31);
}
static void mutations(void) {
  const char* names[] = {"blank-a6x-3.15.27-two-pages.note",
                         "heading-n5-20260016-backgrounds-marker.note",
                         "test-a5x-20220011-old-pen-ids.note",
                         "erase-n6-20230015-horizontal-1270.note"};
  for (size_t fixture_index = 0; fixture_index < 4; fixture_index++) {
    char path[1024];
    snprintf(path, sizeof path, "%s/supernote-typescript/%s", SN_FIXTURE_DIR,
             names[fixture_index]);
    size_t size = 0;
    uint8_t* original = sn_test_read_file(path, &size);
    SN_CHECK(original != nullptr, "fixture");
    if (!original) continue;
    uint8_t* data = malloc(size);
    if (!data) {
      free(original);
      continue;
    }
    uint64_t state = 0x5eed;
    for (size_t iteration = 0; iteration < 600; iteration++) {
      memcpy(data, original, size);
      size_t length = size;
      for (size_t mutation_index = 0; mutation_index < 1 + iteration % 8;
           mutation_index++) {
        size_t index = (size_t)(next(&state) % size);
        data[index] = (uint8_t)next(&state);
      }
      if (iteration % 10 == 0) length = (size_t)(next(&state) % size);
      sn_notebook* notebook = nullptr;
      sn_status status = sn_parse_notebook(data, length, &notebook, nullptr);
      SN_CHECK(sn_test_work_used() <= sn_test_work_limit(),
               "mutated input work never exceeds ceiling");
      SN_CHECK(status != SN_OK || sn_test_work_used() <= sn_test_work_limit(),
               "successful parse respects work ceiling");
      SN_CHECK((status == SN_OK) == (notebook != nullptr), "mutation result");
      if (notebook) walk_sn_notebook(notebook);
      sn_notebook_free(notebook);
    }
    free(data);
    free(original);
  }
}

static void bitmap_mutations(void) {
  char path[1024];
  snprintf(path, sizeof path,
           "%s/supernote-typescript/color-n6-20230015-unknown-palette.note",
           SN_FIXTURE_DIR);
  size_t size = 0;
  uint8_t* file = sn_test_read_file(path, &size);
  if (!file) {
    SN_CHECK(false, "mutation seed");
    return;
  }
  sn_notebook* notebook = nullptr;
  sn_status status = sn_parse_notebook(file, size, &notebook, nullptr);
  free(file);
  SN_CHECK(status == SN_OK, "mutation notebook");
  if (!notebook) return;
  const sn_bitmap* template = nullptr;
  for (size_t i = 0; i < notebook->template_count; i++)
    if (sn_equal(notebook->templates[i].name, "style_task_list"))
      template = &notebook->templates[i].bitmap;
  SN_CHECK(template != nullptr, "template seed");
  uint64_t state = 0xb17;
  if (template)
    for (size_t iteration = 0; iteration < 300; iteration++) {
      uint8_t data[364];
      size_t length = 64 + iteration;
      if (length > template->bytes.size) length = template->bytes.size;
      memcpy(data, template->bytes.data, length);
      for (size_t mutation_index = 0; mutation_index < 1 + iteration % 4;
           mutation_index++) {
        size_t index = (size_t)(next(&state) % length);
        data[index] = (uint8_t)next(&state);
      }
      sn_bitmap bitmap = {.bytes = {data, length}};
      sn_raster* raster = nullptr;
      status = sn_bitmap_raster(&bitmap, 97, 83, nullptr, &raster, nullptr);
      SN_CHECK(sn_test_work_used() <= sn_test_work_limit(),
               "mutated input work never exceeds ceiling");
      SN_CHECK(status != SN_OK || sn_test_work_used() <= sn_test_work_limit(),
               "successful parse respects work ceiling");
      SN_CHECK((status == SN_OK) == (raster != nullptr), "bitmap mutation");
      sn_raster_free(raster);
      bitmap.encoding = SN_ENCODING_PNG;
      SN_CHECK(sn_bitmap_raster(&bitmap, 97, 83, nullptr, &raster, nullptr) ==
                   SN_ERR_IMAGE_ENCODING,
               "encoded image mutation");
    }
  sn_notebook_free(notebook);
}
static void small_mutations(void) {
  for (size_t kind = 0; kind < 7; kind++) {
    char path[1024];
    snprintf(path, sizeof path, "%s/seed-%zu.%s", SN_SEED_DIR, kind,
             kind == 5 ? "mark" : "note");
    generated seed = {0};
    seed.data = sn_test_read_file(path, &seed.size);
    SN_CHECK(seed.data != nullptr, "small seed file");
    if (!seed.data) continue;
    uint8_t* data = malloc(seed.size);
    if (!data) abort();
    uint64_t state = kind + 1;
    for (size_t iteration = 0; iteration < 2000; iteration++) {
      memcpy(data, seed.data, seed.size);
      if (iteration) data[next(&state) % seed.size] ^= (uint8_t)next(&state);
      sn_notebook* notebook = nullptr;
      sn_status status = sn_parse_notebook(data, seed.size, &notebook, nullptr);
      SN_CHECK(sn_test_work_used() <= sn_test_work_limit(),
               "mutated input work never exceeds ceiling");
      SN_CHECK(status != SN_OK || sn_test_work_used() <= sn_test_work_limit(),
               "successful parse respects work ceiling");
      SN_CHECK((status == SN_OK) == (notebook != nullptr),
               "small mutation output");
      if (notebook) walk_sn_notebook(notebook);
      sn_notebook_free(notebook);
    }
    free(data);
    free(seed.data);
  }
}

int main(void) {
  small_mutations();
  mutations();
  bitmap_mutations();
  return sn_test_report("mutation");
}
