/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */
#include "sn_generate.h"
#include "sn_test.h"
/* Apply the work invariant to every parse shape in this suite, including
 * successful large/shared cases and malformed optional content. */
static sn_status budget_parse(const uint8_t* data, size_t size,
                              sn_notebook** out, sn_error* error) {
  sn_status status = sn_parse_notebook(data, size, out, error);
  SN_CHECK(sn_test_work_used() <= sn_test_work_limit(),
           "every shape stays within its work ceiling");
  SN_CHECK(status != SN_OK || sn_test_work_used() <= sn_test_work_limit(),
           "successful shape respects work ceiling");
  return status;
}
static size_t log_calls;
static void count_log(sn_log_level level, const char* message, void* context) {
  (void)level;
  (void)message;
  (void)context;
  log_calls++;
}
static void optional_fields(void) {
  generated seed = gen_seed(4);
  sn_notebook* notebook = nullptr;
  sn_status status = budget_parse(seed.data, seed.size, &notebook, nullptr);
  SN_CHECK(status == SN_OK && notebook && notebook->link_count == 1,
           "link seed");
  if (notebook && notebook->link_count)
    SN_CHECK(sn_equal(notebook->links[0].text, "link"),
             "strip one leading link BOM");
  sn_notebook_free(notebook);
  free(seed.data);
  generated file = gen_begin(), footer = {0};
  const uint8_t pixel[] = {0x62, 0};
  size_t cover = gen_block(&file, pixel, sizeof pixel);
  gen_key(&footer, "COVER_", 1, cover);
  gen_key(&footer, "COVER_", 2, UINT32_MAX);
  gen_finish(&file, &footer);
  sn_log_install(count_log, nullptr);
  notebook = nullptr;
  status = budget_parse(file.data, file.size, &notebook, nullptr);
  sn_log_silence();
  SN_CHECK(status == SN_OK && notebook && notebook->cover_type == 1,
           "first cover retained");
  SN_CHECK(log_calls == 0, "later cover is neither read nor diagnosed");
  sn_notebook_free(notebook);
  free(file.data);
}
static void arena_requests(void) {
  sn_budget budget = {.limit = SN_PARSE_BUDGET_FLOOR};
  sn_arena arena = {.budget = &budget};
  SN_CHECK(
      sn_alloc(&arena, 0, sizeof(sn_stroke)) != nullptr && budget.live == 0,
      "empty array costs nothing");
  sn_test_alloc_fail_after(1);
  void* first = sn_alloc(&arena, 1, sizeof(sn_stroke));
  SN_CHECK(first && (uintptr_t)first % _Alignof(sn_stroke) == 0,
           "aligned bump allocation");
  SN_CHECK(!sn_alloc(&arena, 1, sizeof(sn_stroke)) && sn_test_alloc_did_fail(),
           "logical request can fail inside a chunk");
  sn_arena_free(&arena);
  SN_CHECK(budget.live == 0, "chunks released from live budget");
  arena = (sn_arena){.budget = &budget};
  SN_CHECK(!sn_alloc(&arena, SIZE_MAX, 2) && budget.exceeded,
           "overflow exceeds budget");
  sn_arena_free(&arena);
}
static size_t log_bound(size_t count);
/* Counters measure parser work independently of machine speed. */
static void linear_work(void) {
  constexpr size_t count = 200000;
  for (size_t shape = 0; shape < 4; shape++) {
    generated file = gen_begin(), footer = {0};
    size_t shared = 0;
    if (shape >= 2) {
      generated metadata = {0};
      if (shape == 3)
        for (size_t index = 0; index < count; index++)
          gen_key(&metadata, "OTHER", index, 0);
      shared = gen_block(&file, metadata.data, metadata.size);
      free(metadata.data);
    }
    for (size_t index = 0; index < count; index++) {
      size_t address = shape >= 2 ? shared : gen_tags(&file, "<X:0>");
      gen_key(&footer, shape == 0 || shape == 2 ? "PAGE" : "TITLE_", index + 1,
              address);
    }
    gen_finish(&file, &footer);
    sn_test_work_reset();
    sn_notebook* notebook = nullptr;
    sn_error error;
    sn_status status = budget_parse(file.data, file.size, &notebook, &error);
    SN_CHECK(status == SN_OK, "linear shape %zu: %s", shape, error.message);
    SN_CHECK(sn_test_cache_probes() < 16 * count * log_bound(count),
             "cache probes %zu", sn_test_cache_probes());
    SN_CHECK(sn_test_metadata_comparisons < 32 * count,
             "metadata comparisons %zu", sn_test_metadata_comparisons);
    SN_CHECK(!sn_test_duplicate_decodes(), "one decode per tuple");
    if (notebook)
      SN_CHECK((shape == 0 || shape == 2 ? notebook->page_count
                                         : notebook->title_count) == count,
               "all keys retained");
    printf("linear shape %zu probes %zu comparisons %zu\n", shape,
           sn_test_cache_probes(), sn_test_metadata_comparisons);
    sn_notebook_free(notebook);
    free(file.data);
  }
}

static void link_bom(void) {
  generated file = gen_begin(), footer = {0};
  size_t address =
      gen_tags(&file, "<LINKTYPE:4><LINKFILE:77u/eA==><SHOWTEXT:77u/eA==>");
  gen_key(&footer, "LINK_", 10001, address);
  gen_finish(&file, &footer);
  sn_notebook* notebook = nullptr;
  SN_CHECK(budget_parse(file.data, file.size, &notebook, nullptr) == SN_OK,
           "BOM link parses");
  if (notebook && notebook->link_count == 1) {
    SN_CHECK(sn_equal(notebook->links[0].text, "x"), "display BOM removed");
    SN_CHECK(sn_equal(notebook->links[0].target_path, "\xef\xbb\xbfx") &&
                 sn_equal(notebook->links[0].url, "\xef\xbb\xbfx"),
             "target BOM retained");
  } else
    SN_CHECK(false, "BOM link retained");
  sn_notebook_free(notebook);
  free(file.data);
}

static size_t log_bound(size_t count) {
  size_t result = 1;
  while (count >>= 1) result++;
  return result;
}
static void cache_orders(void) {
  constexpr size_t count = 500000;
  for (size_t order = 0; order < 4; order++) {
    sn_arena arena = {0};
    sn_test_work_reset();
    for (size_t index = 0; index < count; index++) {
      size_t key = order == 0   ? index
                   : order == 1 ? count - index - 1
                   : order == 2
                       ? (index % 2 ? count - 1 - index / 2 : index / 2)
                       : 0;
      size_t context = order == 3 ? index : 0;
      size_t before = sn_test_cache_probes();
      sn_decode_begin(&arena, key, SN_CACHE_PAGE, context);
      sn_cache_put(&arena, key, SN_CACHE_PAGE, context, &index, sizeof index);
      SN_CHECK(sn_test_cache_probes() - before <= 4 * log_bound(index + 1),
               "logarithmic insertion");
      before = sn_test_cache_probes();
      size_t* found = sn_cached(&arena, key, 2, context);
      SN_CHECK(found && *found == index, "every insertion retained");
      SN_CHECK(sn_test_cache_probes() - before <= 2 * log_bound(index + 1),
               "logarithmic lookup");
    }
    size_t steps = sn_test_cache_probes();
    SN_CHECK(arena.cache_count == count && sn_test_cache_walk(&arena),
             "ordered complete balanced walk");
    SN_CHECK(sn_test_decodes() == count && !sn_test_duplicate_decodes(),
             "one decode per key");
    printf("index order %zu keys %zu steps %zu decodes %zu\n", order, count,
           steps, sn_test_decodes());
    sn_arena_free(&arena);
  }
}
static void heavy_page(void) {
  generated file = gen_begin(), footer = {0}, metadata = {0};
  constexpr size_t entries = 100000;
  constexpr size_t keys = 10000;
  constexpr size_t distinct = 4096;
  for (size_t index = 0; index < entries; index++)
    gen_key(&metadata, "OTHER", index, 0);
  size_t heavy = gen_block(&file, metadata.data, metadata.size);
  free(metadata.data);
  for (size_t index = 0; index < distinct; index++)
    gen_key(&footer, "PAGE", index + 1, gen_tags(&file, "<X:0>"));
  for (size_t index = 0; index < keys; index++)
    gen_key(&footer, "PAGE", distinct + index + 1, heavy);
  gen_finish(&file, &footer);
  size_t steps = 0, comparisons = 0, decodes = 0;
  sn_notebook* first = nullptr;
  for (size_t layout = 0; layout < 2; layout++) {
    void* padding[64] = {0};
    if (layout) {
      for (size_t index = 0; index < 64; index++)
        padding[index] = malloc(1024 + index * 7919);
      for (size_t index = 0; index < 64; index += 2) {
        free(padding[index]);
        padding[index] = nullptr;
      }
    }
    sn_test_work_reset();
    sn_notebook* notebook = nullptr;
    SN_CHECK(budget_parse(file.data, file.size, &notebook, nullptr) == SN_OK,
             "heavy page parses");
    SN_CHECK(notebook && notebook->page_count == keys + distinct,
             "all page aliases retained");
    SN_CHECK(!sn_test_duplicate_decodes(), "no tuple decoded twice");
    SN_CHECK(sn_test_kind_decodes(SN_CACHE_PAGE) == distinct + 1,
             "heavy page decoded exactly once");
    SN_CHECK(sn_test_cache_probes() <
                 16 * (entries + (keys + distinct) * log_bound(distinct)),
             "heavy index bound");
    SN_CHECK(sn_test_metadata_comparisons < 12 * (entries + keys + distinct),
             "single metadata pass");
    if (notebook)
      SN_CHECK(notebook->pages[distinct].layers ==
                   notebook->pages[distinct + keys - 1].layers,
               "heavy decode shared");
    if (!layout) {
      steps = sn_test_cache_probes();
      comparisons = sn_test_metadata_comparisons;
      decodes = sn_test_decodes();
      first = notebook;
    } else {
      SN_CHECK(first && notebook && first->pages != notebook->pages,
               "distinct live heap layouts");
      SN_CHECK(steps == sn_test_cache_probes() &&
                   comparisons == sn_test_metadata_comparisons &&
                   decodes == sn_test_decodes(),
               "allocator-independent operation sequence");
      sn_notebook_free(notebook);
    }
    for (size_t index = 0; index < 64; index++) free(padding[index]);
  }
  printf(
      "heavy entries %zu keys %zu distinct %zu steps %zu comparisons %zu "
      "decodes %zu peak %zu\n",
      entries, keys, distinct, steps, comparisons, decodes,
      sn_test_arena_peak());
  sn_notebook_free(first);
  free(file.data);
}
static void protocol_ids(void) {
  uint8_t bytes[] = {0x62, 0};
  sn_arena arena = {.file_base = bytes, .file_size = sizeof bytes};
  char first[] = "CUSTOM", second[] = "CUSTOM", other[] = "CUSTOM2";
  sn_test_work_reset();
  const sn_bitmap* a = sn_bitmap_decode(&arena, (sn_bytes){bytes, sizeof bytes},
                                        (sn_string){first, 6});
  const sn_bitmap* b = sn_bitmap_decode(&arena, (sn_bytes){bytes, sizeof bytes},
                                        (sn_string){second, 6});
  const sn_bitmap* c = sn_bitmap_decode(&arena, (sn_bytes){bytes, sizeof bytes},
                                        (sn_string){other, 7});
  SN_CHECK(a && b && c && a == b && a != c,
           "protocol identity uses bytes and distinguishes prefixes");
  SN_CHECK(
      a && c && sn_equal(a->name, "CUSTOM") && sn_equal(c->name, "CUSTOM2"),
      "protocol names retained");
  SN_CHECK(sn_test_kind_decodes(SN_CACHE_BITMAP) == 1 &&
               sn_test_kind_decodes(SN_CACHE_BITMAP_VIEW) == 2 &&
               !sn_test_duplicate_decodes(),
           "one interpretation per protocol");
  sn_arena_free(&arena);
}
static void empty_blocks(void) {
  generated file = gen_begin(), footer = {0};
  size_t first = gen_tags(&file, ""), second = gen_tags(&file, "nothing");
  gen_key(&footer, "KEYWORD_", 10001, first);
  gen_key(&footer, "KEYWORD_", 20001, second);
  gen_key(&footer, "KEYWORD_", 30001, first);
  gen_finish(&file, &footer);
  sn_notebook* notebook = nullptr;
  SN_CHECK(budget_parse(file.data, file.size, &notebook, nullptr) == SN_OK,
           "empty metadata");
  if (notebook && notebook->keyword_count == 3) {
    SN_CHECK(
        notebook->keywords[0].text.data != notebook->keywords[1].text.data &&
            notebook->keywords[0].text.data == notebook->keywords[2].text.data,
        "block identity, not empty sentinel");
    SN_CHECK(notebook->keywords[0].page_number == 1000 &&
                 notebook->keywords[2].page_number == 3000,
             "shared keyword keeps per-key page");
  } else
    SN_CHECK(false, "empty keywords retained");
  sn_notebook_free(notebook);
  free(file.data);
}

/* Distinct length words overlap the same one-mebibyte payload. */
static generated overlapping(bool layers) {
  generated file = gen_begin(), footer = {0};
  size_t region = file.size, count = layers ? 10000 : 20000;
  constexpr size_t length = 1048576;
  for (size_t index = 0; index < length / 4 + count + 1; index++)
    gen_u32(&file, (uint32_t)length);
  for (size_t index = 0; index < count; index++) {
    size_t address = region + 4 * index;
    if (layers) {
      char text[128];
      snprintf(text, sizeof text, "<LAYERBITMAP:%zu>", address);
      size_t layer = gen_tags(&file, text);
      snprintf(text, sizeof text, "<MAINLAYER:%zu>", layer);
      address = gen_tags(&file, text);
    }
    gen_key(&footer, "PAGE", index + 1, address);
  }
  gen_finish(&file, &footer);
  return file;
}
static void overlapping_work(void) {
  for (size_t shape = 0; shape < 2; shape++) {
    generated file = overlapping(shape != 0);
    sn_notebook* sentinel = (sn_notebook*)(uintptr_t)1;
    sn_notebook* notebook = sentinel;
    sn_error error;
    sn_status status = budget_parse(file.data, file.size, &notebook, &error);
    SN_CHECK(status == SN_ERR_LIMIT && notebook == sentinel &&
                 strstr(error.message, "work budget"),
             "overlapping shape %zu: %s", shape, error.message);
    SN_CHECK(sn_test_work_used() <= sn_test_work_limit(),
             "overlap charged within ceiling");
    printf("overlap %zu input %zu peak %zu work %zu ceiling %zu status %d\n",
           shape, file.size, sn_test_arena_peak(), sn_test_work_used(),
           sn_test_work_limit(), (int)status);
    free(file.data);
  }
}
static void distinct_protocols(void) {
  constexpr size_t pages = 8000;
  const size_t repeated[] = {39999, 0, 20000, 1, 39999};
  const size_t order[] = {3, 2, 1, 0, 4};
  generated file = gen_begin(), footer = {0};
  const uint8_t pixel[] = {0x61, 0x10};
  size_t bitmap = gen_block(&file, pixel, sizeof pixel);
  for (size_t page = 0; page <= pages; page++) {
    size_t layers[5];
    char text[256];
    for (size_t layer = 0; layer < 5; layer++) {
      snprintf(text, sizeof text, "<LAYERBITMAP:%zu><LAYERPROTOCOL:X%07zu>",
               bitmap, page == pages ? repeated[layer] : page * 5 + layer);
      layers[layer] = gen_tags(&file, text);
    }
    snprintf(text, sizeof text,
             "<MAINLAYER:%zu><LAYER1:%zu><LAYER2:%zu><LAYER3:%zu><BGLAYER:%zu>",
             layers[0], layers[1], layers[2], layers[3], layers[4]);
    gen_key(&footer, "PAGE", page + 1, gen_tags(&file, text));
  }
  gen_finish(&file, &footer);
  sn_test_work_reset();
  sn_notebook* notebook = nullptr;
  sn_status status = budget_parse(file.data, file.size, &notebook, nullptr);
  SN_CHECK(status == SN_OK && notebook && notebook->page_count == pages + 1,
           "40,000 distinct protocols parse");
  if (notebook && notebook->page_count == pages + 1) {
    for (size_t page = 0; page <= pages; page++) {
      const sn_page* value = &notebook->pages[page];
      SN_CHECK(value->number == (int64_t)(page + 1) && value->layer_count == 5,
               "protocol page and layer order %zu", page);
      for (size_t layer = 0; layer < value->layer_count; layer++) {
        char expected[32];
        size_t id =
            page == pages ? repeated[order[layer]] : page * 5 + order[layer];
        snprintf(expected, sizeof expected, "X%07zu", id);
        const sn_bitmap* image = value->layers[layer].bitmap;
        SN_CHECK(value->layers[layer].name == (sn_layer_name)order[layer] &&
                     image && image->encoding == SN_ENCODING_OTHER &&
                     sn_equal(image->name, expected) &&
                     image->bytes.size == sizeof pixel &&
                     memcmp(image->bytes.data, pixel, sizeof pixel) == 0,
                 "protocol spelling and encounter order %zu/%zu", page, layer);
        if (page == pages)
          SN_CHECK(
              image == notebook->pages[id / 5].layers[order[id % 5]].bitmap,
              "duplicate name at a different file offset shares view");
      }
    }
  }
  SN_CHECK(sn_test_kind_decodes(SN_CACHE_BITMAP) == 1 &&
               sn_test_kind_decodes(SN_CACHE_BITMAP_VIEW) == pages * 5 &&
               !sn_test_duplicate_decodes(),
           "one shared bitmap and one view per distinct protocol");
  SN_CHECK(sn_test_work_used() < sn_test_work_limit() / 4,
           "distinct protocol work below one quarter of ceiling");
  printf("distinct protocols 40000 input %zu work %zu ceiling %zu status %d\n",
         file.size, sn_test_work_used(), sn_test_work_limit(), (int)status);
  sn_notebook_free(notebook);
  free(file.data);
}
/* Ordered and common-prefix names must retain encounter IDs with a
 * logarithmic bound per operation, independently of input/heap layout. */
static void protocol_orders(void) {
  constexpr size_t count = 20000;
  char (*names)[33] = calloc(count, sizeof *names);
  SN_CHECK(names != nullptr, "protocol name storage");
  if (!names) return;
  for (size_t order = 0; order < 4; order++) {
    sn_arena arena = {0};
    sn_test_work_reset();
    for (size_t index = 0; index < count; index++) {
      size_t key = order == 0   ? index
                   : order == 1 ? count - index - 1
                   : index % 2  ? count - 1 - index / 2
                                : index / 2;
      snprintf(names[index], sizeof *names, "%027u%05zu", 0u, key);
      size_t length = order == 3 ? 28 + index % 5 : 32;
      /* Mixed lengths keep the unique numeric suffix. */
      sn_string name = {names[index] + 32 - length, length};
      size_t before = sn_test_cache_probes();
      SN_CHECK(sn_protocol(&arena, name) == index + 1,
               "first encounter assigns ID %zu/%zu", order, index);
      SN_CHECK(sn_test_cache_probes() - before <= 6 * log_bound(index + 1),
               "protocol insertion has logarithmic traversal");
      char duplicate[33];
      memcpy(duplicate, name.data, length);
      before = sn_test_cache_probes();
      SN_CHECK(sn_protocol(&arena, (sn_string){duplicate, length}) == index + 1,
               "duplicate bytes at a different address retain ID");
      SN_CHECK(sn_test_cache_probes() - before <= 2 * log_bound(index + 1),
               "protocol duplicate has logarithmic traversal");
    }
    for (size_t index = 0; index < count; index++) {
      size_t length = order == 3 ? 28 + index % 5 : 32;
      SN_CHECK(sn_protocol(&arena, (sn_string){names[index] + 32 - length,
                                               length}) == index + 1,
               "ID survives all subsequent tree rotations");
    }
    SN_CHECK(arena.protocol_count == count && arena.cache_count == count &&
                 sn_test_cache_walk(&arena),
             "every distinct name has one balanced index entry");
    sn_arena_free(&arena);
  }
  free(names);
}
static void long_protocols(void) {
  generated file = gen_begin(), footer = {0}, page = {0};
  const uint8_t pixel[] = {0x61, 0x10};
  size_t bitmap = gen_block(&file, pixel, sizeof pixel);
  const char* names[] = {"MAINLAYER", "LAYER1", "LAYER2", "LAYER3", "BGLAYER"};
  for (size_t layer = 0; layer < 5; layer++) {
    generated metadata = {0};
    char text[128];
    int length =
        snprintf(text, sizeof text, "<LAYERBITMAP:%zu><LAYERPROTOCOL:", bitmap);
    gen_append(&metadata, text, (size_t)length);
    for (size_t index = 0; index <= 200000; index++) {
      uint8_t byte = index ? (uint8_t)(48 + (index * 7 + layer) % 74)
                           : (uint8_t)('A' + layer);
      if (byte == '<') byte = 'x';
      if (byte == '>') byte = 'y';
      if (byte == ':') byte = 'z';
      gen_append(&metadata, &byte, 1);
    }
    gen_append(&metadata, ">", 1);
    size_t address = gen_block(&file, metadata.data, metadata.size);
    free(metadata.data);
    length = snprintf(text, sizeof text, "<%s:%zu>", names[layer], address);
    gen_append(&page, text, (size_t)length);
  }
  gen_key(&footer, "PAGE", 1, gen_block(&file, page.data, page.size));
  free(page.data);
  gen_finish(&file, &footer);
  sn_notebook* notebook = nullptr;
  sn_status status = budget_parse(file.data, file.size, &notebook, nullptr);
  SN_CHECK(status == SN_OK && notebook, "long protocols parse");
  SN_CHECK(sn_test_arena_peak() <= 5 * file.size, "protocol storage bounded");
  SN_CHECK(sn_test_work_used() <= sn_test_work_limit() / 8,
           "protocol work margin");
  if (notebook && notebook->page_count == 1) {
    SN_CHECK(notebook->pages[0].layer_count == 5, "all protocol layers");
    for (size_t index = 0; index < notebook->pages[0].layer_count; index++) {
      const sn_bitmap* value = notebook->pages[0].layers[index].bitmap;
      SN_CHECK(value && value->encoding == SN_ENCODING_OTHER &&
                   value->name.length == 200001,
               "long name retained");
    }
  }
  printf("protocol input %zu peak %zu work %zu ceiling %zu\n", file.size,
         sn_test_arena_peak(), sn_test_work_used(), sn_test_work_limit());
  sn_notebook_free(notebook);
  free(file.data);
}
static void uncached_slices(void) {
  uint8_t bytes[8] = {0x62, 0};
  sn_arena arena = {.file_base = bytes + 2, .file_size = 2};
  SN_CHECK(sn_offset(&arena, bytes) == SIZE_MAX &&
               sn_offset(&arena, bytes + 5) == SIZE_MAX &&
               sn_offset(&arena, nullptr) == SIZE_MAX &&
               sn_offset(&arena, bytes + 4) == 2,
           "slice bounds");
  const sn_bitmap* value =
      sn_bitmap_decode(&arena, (sn_bytes){bytes, 2}, (sn_string){0});
  SN_CHECK(value && arena.cache_count == 0, "external slice decoded uncached");
  arena.file_base = nullptr;
  SN_CHECK(sn_offset(&arena, bytes) == SIZE_MAX, "null file base");
  sn_arena_free(&arena);
}
static void protocol_boundary(void) {
  sn_arena arena = {0};
  char first[34], second[34];
  memset(first, 'x', sizeof first);
  memset(second, 'x', sizeof second);
  size_t id = sn_protocol(&arena, (sn_string){first, 32});
  SN_CHECK(id != SIZE_MAX &&
               sn_protocol(&arena, (sn_string){second, 32}) == id &&
               arena.protocol_count == 1,
           "whole equal 32-byte protocols");
  SN_CHECK(sn_protocol(&arena, (sn_string){first, 33}) == SIZE_MAX &&
               arena.protocol_count == 1,
           "33-byte protocol never interned");
  sn_arena_free(&arena);

  /* Protocol-name boundary case: equal 32-byte names, a difference
   * at byte 32, and two 33-byte names differing only after the boundary. */
  const char* names[] = {
      "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP", "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
      "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPQ", "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPA",
      "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPB"};
  const char* keys[] = {"MAINLAYER", "LAYER1", "LAYER2", "LAYER3", "BGLAYER"};
  const size_t order[] = {3, 2, 1, 0, 4};
  generated file = gen_begin(), page = {0}, footer = {0};
  const uint8_t pixel[] = {0x61, 0x10};
  size_t bitmap = gen_block(&file, pixel, sizeof pixel);
  for (size_t layer = 0; layer < 5; layer++) {
    char text[128];
    snprintf(text, sizeof text, "<LAYERBITMAP:%zu><LAYERPROTOCOL:%s>", bitmap,
             names[layer]);
    size_t address = gen_tags(&file, text);
    int length = snprintf(text, sizeof text, "<%s:%zu>", keys[layer], address);
    gen_append(&page, text, (size_t)length);
  }
  gen_key(&footer, "PAGE", 1, gen_block(&file, page.data, page.size));
  free(page.data);
  gen_finish(&file, &footer);
  sn_notebook* notebook = nullptr;
  SN_CHECK(budget_parse(file.data, file.size, &notebook, nullptr) == SN_OK &&
               notebook && notebook->page_count == 1 &&
               notebook->pages[0].layer_count == 5,
           "protos-boundary parses");
  if (notebook && notebook->page_count == 1 &&
      notebook->pages[0].layer_count == 5) {
    const sn_layer* layers = notebook->pages[0].layers;
    for (size_t layer = 0; layer < 5; layer++)
      SN_CHECK(layers[layer].bitmap &&
                   layers[layer].bitmap->encoding == SN_ENCODING_OTHER &&
                   sn_equal(layers[layer].bitmap->name, names[order[layer]]),
               "entire boundary protocol retained %zu", layer);
    SN_CHECK(layers[3].bitmap == layers[2].bitmap &&
                 layers[3].bitmap != layers[1].bitmap &&
                 layers[0].bitmap != layers[4].bitmap,
             "equal names dedup; byte 32 and long suffixes stay distinct");
  }
  sn_notebook_free(notebook);
  free(file.data);
}
static void protocol_storage_limit(void) {
  sn_budget budget = {.limit = SN_PARSE_BUDGET_FLOOR};
  sn_arena arena = {.budget = &budget};
  /* Keep borrowed names alive until arena teardown. The real chunk budget,
   * rather than a name-count cap, must stop this isolated intern workload. */
  constexpr size_t count = 40000;
  char (*names)[16] = calloc(count, sizeof *names);
  SN_CHECK(names != nullptr, "bounded protocol storage names");
  if (!names) return;
  size_t inserted = 0;
  for (; inserted < count; inserted++) {
    int length = snprintf(names[inserted], sizeof *names, "P%zu", inserted);
    if (sn_protocol(&arena, (sn_string){names[inserted], (size_t)length}) ==
        SIZE_MAX)
      break;
  }
  SN_CHECK(inserted > 0 && inserted < count && arena.failed &&
               budget.exceeded && budget.live <= budget.limit &&
               arena.protocol_count == inserted,
           "intern population bounded by actual arena capacity");
  sn_arena_free(&arena);
  SN_CHECK(budget.live == 0, "protocol storage budget released");
  free(names);

  arena = (sn_arena){0};
  sn_test_alloc_fail_after(0);
  SN_CHECK(sn_protocol(&arena, (sn_string){"NEW", 3}) == SIZE_MAX &&
               arena.failed && arena.protocol_count == 0 &&
               sn_test_alloc_did_fail(),
           "failed node allocation publishes no protocol ID");
  sn_test_alloc_fail_after(SIZE_MAX);
  sn_arena_free(&arena);
}
/* Exercise the escape with simultaneous scratch chunks, then confirm that
 * normal release removes a chunk from the emergency cleanup ledger. */
static void escape_scratch(sn_work_meter* meter) {
  if (setjmp(meter->escape)) return;
  sn_arena first = {0}, second = {0};
  SN_CHECK(sn_alloc(&first, 1, 1) && sn_alloc(&second, 1, 1),
           "scratch ledger allocations");
  sn_arena_free(&first);
  SN_CHECK(meter->chunks != nullptr, "remaining scratch tracked");
  sn_work(meter->limit);
  SN_CHECK(false, "failed charge must escape immediately");
}
static void work_cleanup(void) {
  sn_work_meter meter = {.limit = 2};
  sn_meter = &meter;
  escape_scratch(&meter);
  SN_CHECK(meter.used == 2 && meter.used <= meter.limit,
           "rejected charge leaves counter within ceiling");
  sn_work_release(&meter, true);
  SN_CHECK(!meter.chunks && !meter.owner, "escape releases all scratch");
  sn_meter = nullptr;
}
typedef enum scan_kind {
  SCAN_METADATA,
  SCAN_JSON,
  SCAN_BASE64,
  SCAN_TRIM,
  SCAN_TEXT,
  SCAN_INTEGER,
} scan_kind;
typedef struct scan_case {
  const char* name;
  scan_kind kind;
  sn_bytes input;
  size_t limit;
  bool allocated;
} scan_case;
/* Keep the meter in the caller: modified scanner locals are indeterminate
 * after longjmp. This is the same escape that parse_metered maps to LIMIT. */
static sn_status escape_scan(sn_work_meter* meter, const scan_case* test) {
  if (setjmp(meter->escape)) return SN_ERR_LIMIT;
  sn_arena arena = {0};
  sn_bytes decoded;
  sn_string text = {(const char*)test->input.data, test->input.size};
  int64_t integer;
  switch (test->kind) {
    case SCAN_METADATA:
      (void)sn_metadata_read(&arena, test->input);
      break;
    case SCAN_JSON:
      (void)sn_json_read(&arena, test->input);
      break;
    case SCAN_BASE64:
      (void)sn_base64(&arena, test->input, false, &decoded);
      break;
    case SCAN_TRIM:
      (void)sn_trim(text, true);
      break;
    case SCAN_TEXT:
      (void)sn_text(&arena, test->input.data, test->input.size);
      break;
    case SCAN_INTEGER:
      (void)sn_integer(text, &integer);
      break;
  }
  sn_arena_free(&arena);
  return SN_OK;
}
/* Each ceiling lies inside the named loop, beyond any earlier scans and
 * allocation charges. In particular, the second passes must retain their live
 * chunks on the caller-owned emergency cleanup ledger when they escape. */
static void scanner_limits(void) {
  uint8_t spaces[64], padding[64], digits[64], utf16[130];
  memset(spaces, ' ', sizeof spaces);
  memset(padding, '=', sizeof padding);
  /* Leading zeroes avoid integer overflow before the work ceiling. */
  memset(digits, '0', sizeof digits);
  utf16[0] = 0xff;
  utf16[1] = 0xfe;
  for (size_t index = 2; index < sizeof utf16; index += 2) {
    utf16[index] = index == 2 || index == sizeof utf16 - 2 ? '"' : 'a';
    utf16[index + 1] = 0;
  }
  const uint8_t key[] =
      "<aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:v>";
  const uint8_t value[] =
      "<k:vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv>";
  const uint8_t string[] =
      "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"";
  const uint8_t escapes[] =
      "\"\\u0061\\u0061\\u0061\\u0061\\u0061\\u0061\\u0061\\u0061\"";
  const uint8_t number[] =
      "1111111111111111111111111111111111111111111111111111111111111111";
  const uint8_t fraction[] =
      "0.1111111111111111111111111111111111111111111111111111111111111111";
  const uint8_t exponent[] =
      "0e1111111111111111111111111111111111111111111111111111111111111111";
  const uint8_t base64[] =
      "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh";
  const uint8_t unicode[] =
      "\xf0\x9f\x99\x82\xf0\x9f\x99\x82\xf0\x9f\x99\x82\xf0\x9f\x99\x82"
      "\xf0\x9f\x99\x82\xf0\x9f\x99\x82\xf0\x9f\x99\x82\xf0\x9f\x99\x82";
  const scan_case tests[] = {
      {"metadata count", SCAN_METADATA, {spaces, sizeof spaces}, 17, false},
      {"JSON whitespace", SCAN_JSON, {spaces, sizeof spaces}, 17, false},
      {"Base64 count", SCAN_BASE64, {padding, sizeof padding}, 17, false},
      {"trim outer", SCAN_TRIM, {spaces, sizeof spaces}, 17, false},
      {"text outer",
       SCAN_TEXT,
       {spaces, sizeof spaces},
       sizeof spaces * 3 + 1 + 17,
       true},
      {"integer digits", SCAN_INTEGER, {digits, sizeof digits}, 17, false},
      {"UTF-16 outer",
       SCAN_JSON,
       {utf16, sizeof utf16},
       sizeof utf16 * 3 + 16,
       true},
      {"UTF-16 inner",
       SCAN_JSON,
       {utf16, sizeof utf16},
       sizeof utf16 * 3 + 18,
       true},
      {"JSON string scan",
       SCAN_JSON,
       {string, sizeof string - 1},
       1 + sizeof(sn_json) + 17,
       true},
      {"JSON string decode",
       SCAN_JSON,
       {string, sizeof string - 1},
       sizeof(sn_json) + 2 * (sizeof string - 1) + 17,
       true},
      {"JSON hex quad",
       SCAN_JSON,
       {escapes, sizeof escapes - 1},
       sizeof(sn_json) + 2 * (sizeof escapes - 1) + 21,
       true},
      {"JSON integer digits",
       SCAN_JSON,
       {number, sizeof number - 1},
       1 + sizeof(sn_json) + 17,
       true},
      {"JSON fraction digits",
       SCAN_JSON,
       {fraction, sizeof fraction - 1},
       1 + sizeof(sn_json) + 17,
       true},
      {"JSON exponent digits",
       SCAN_JSON,
       {exponent, sizeof exponent - 1},
       1 + sizeof(sn_json) + 17,
       true},
      {"metadata outer",
       SCAN_METADATA,
       {spaces, sizeof spaces},
       sizeof spaces + 1 + 17,
       false},
      {"metadata colon",
       SCAN_METADATA,
       {key, sizeof key - 1},
       sizeof key + sizeof(sn_metadata_entry) + 1 + 17,
       true},
      {"metadata close",
       SCAN_METADATA,
       {value, sizeof value - 1},
       sizeof value + sizeof(sn_metadata_entry) + 3 + 17,
       true},
      {"Base64 decode",
       SCAN_BASE64,
       {base64, sizeof base64 - 1},
       sizeof base64 + (sizeof base64 - 1) / 4 * 3 + 17,
       true},
      {"text continuation",
       SCAN_TEXT,
       {unicode, sizeof unicode - 1},
       (sizeof unicode - 1) * 3 + 1 + 20,
       true},
      {"trim continuation",
       SCAN_TRIM,
       {unicode, sizeof unicode - 1},
       17,
       false},
  };
  for (size_t index = 0; index < sizeof tests / sizeof *tests; index++) {
    const scan_case* test = &tests[index];
    sn_work_meter meter = {.limit = test->limit};
    sn_meter = &meter;
    SN_CHECK(escape_scan(&meter, test) == SN_ERR_LIMIT,
             "%s must escape immediately with LIMIT", test->name);
    SN_CHECK(meter.used == meter.limit,
             "%s stops at the exact per-step ceiling", test->name);
    SN_CHECK((meter.chunks != nullptr) == test->allocated,
             "%s tracks live storage across longjmp", test->name);
    sn_work_release(&meter, true);
    SN_CHECK(!meter.chunks && !meter.owner, "%s cleanup ledger emptied",
             test->name);
    sn_meter = nullptr;
  }
}
static void nested_sink(sn_log_level level, const char* message,
                        void* context) {
  (void)level;
  (void)message;
  size_t* calls = context;
  (*calls)++;
  generated seed = gen_seed(2);
  sn_notebook* notebook = nullptr;
  SN_CHECK(sn_meter == nullptr, "host callback outside parse scope");
  SN_CHECK(budget_parse(seed.data, seed.size, &notebook, nullptr) == SN_OK,
           "nested parse");
  sn_notebook_free(notebook);
  free(seed.data);
}
static void nested_parse(void) {
  generated file = gen_begin(), footer = {0};
  gen_key(&footer, "PAGE", 1, UINT32_MAX);
  gen_key(&footer, "PAGE", 2, gen_tags(&file, "<PAGEID:p>"));
  gen_finish(&file, &footer);
  sn_notebook* notebook = nullptr;
  SN_CHECK(budget_parse(file.data, file.size, &notebook, nullptr) == SN_OK,
           "silent parse");
  size_t work = sn_test_work_used();
  sn_notebook_free(notebook);
  notebook = nullptr;
  size_t calls = 0;
  sn_log_install(nested_sink, &calls);
  SN_CHECK(budget_parse(file.data, file.size, &notebook, nullptr) == SN_OK,
           "outer parse with nested callback");
  sn_log_silence();
  SN_CHECK(calls && sn_test_work_used() == work && sn_meter == nullptr,
           "enclosing work meter restored exactly");
  sn_notebook_free(notebook);
  free(file.data);
}

static void seed_margins(void) {
  for (size_t kind = 0; kind < 7; kind++) {
    generated file = gen_seed(kind);
    sn_notebook* notebook = nullptr;
    sn_status status = budget_parse(file.data, file.size, &notebook, nullptr);
    SN_CHECK(status == SN_OK, "seed %zu", kind);
    SN_CHECK(sn_test_work_used() <= sn_test_work_limit() / 8 &&
                 sn_test_work_used() <= file.size * SN_PARSE_WORK_MULTIPLE / 8,
             "seed work margin %zu", kind);
    printf("seed %zu input %zu peak %zu work %zu ceiling %zu\n", kind,
           file.size, sn_test_arena_peak(), sn_test_work_used(),
           sn_test_work_limit());
    sn_notebook_free(notebook);
    free(file.data);
  }
}

int main(void) {
  work_cleanup();
  scanner_limits();
  protocol_boundary();
  protocol_storage_limit();
  nested_parse();
  seed_margins();
  uncached_slices();
  overlapping_work();
  long_protocols();
  distinct_protocols();
  protocol_orders();
  arena_requests();
  cache_orders();
  heavy_page();
  protocol_ids();
  empty_blocks();
  linear_work();
  link_bom();
  optional_fields();
  for (size_t shape = 0; shape < 7; shape++) {
    generated file = gen_hostile(shape);
    sn_notebook* sentinel = (sn_notebook*)(uintptr_t)1;
    sn_notebook* notebook = sentinel;
    sn_error error;
    sn_test_peak_reset();
    sn_test_work_reset();
    sn_status status = budget_parse(file.data, file.size, &notebook, &error);
    SN_CHECK(sn_test_work_used() <= sn_test_work_limit(),
             "hostile work bounded regardless of status");
    SN_CHECK(status != SN_OK || sn_test_work_used() <= sn_test_work_limit(),
             "successful hostile parse respects ceiling");
    SN_CHECK(!sn_test_duplicate_decodes(), "hostile tuples decoded once");
    size_t budget =
        SN_PARSE_BUDGET_FLOOR + file.size * SN_PARSE_BUDGET_MULTIPLE;
    SN_CHECK(status == (shape == 6 ? SN_ERR_LIMIT : SN_OK), "shape %zu: %s",
             shape, error.message);
    SN_CHECK(sn_test_arena_peak() <= budget, "shape %zu peak %zu budget %zu",
             shape, sn_test_arena_peak(), budget);
    if (status == SN_ERR_LIMIT)
      SN_CHECK(notebook == sentinel && strstr(error.message, "budget"),
               "budget failure output and diagnostic");
    if (status == SN_OK) {
      SN_CHECK(notebook->skipped_pages == 0, "no skipped pages");
      if (shape == 2 || shape == 3 || shape == 5) {
        SN_CHECK(notebook->page_count == 2 &&
                     notebook->pages[0].skipped_strokes == 1 &&
                     notebook->pages[0].stroke_count == 0,
                 "bad log counted");
        SN_CHECK(notebook->pages[1].skipped_strokes == 0 &&
                     sn_equal(notebook->pages[1].id, "good"),
                 "good page intact");
      } else
        SN_CHECK(notebook->pages[0].skipped_strokes == 0,
                 "readable log intact");
    }
    if (shape == 0 && status == SN_OK) {
      SN_CHECK(notebook->page_count == 1000, "all page keys retained");
      SN_CHECK(notebook->pages[0].strokes == notebook->pages[999].strokes,
               "shared stroke result");
    }
    printf("shape %zu input %zu peak %zu budget %zu status %d\n", shape,
           file.size, sn_test_arena_peak(), budget, (int)status);
    if (status == SN_OK) sn_notebook_free(notebook);
    free(file.data);
  }
  return sn_test_report("budget");
}
