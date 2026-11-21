/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_generate.h"
#include "sn_test.h"

/* Every allocation is a possible failure boundary. A sentinel catches a
 * parser that publishes a partially constructed notebook before returning. */
static void check_allocations(const uint8_t* bytes, size_t size) {
  sn_notebook sentinel = {0};
  for (size_t allocation = 0;; allocation++) {
    sn_test_alloc_fail_after(allocation);
    sn_notebook* notebook = &sentinel;
    sn_error error;
    sn_status status = sn_parse_notebook(bytes, size, &notebook, &error);
    bool injected = sn_test_alloc_did_fail();
    SN_CHECK(status == SN_OK || status == SN_ERR_OUT_OF_MEMORY,
             "allocation %zu: %s", allocation, sn_status_string(status));
    if (status == SN_OK) {
      SN_CHECK(!injected, "allocation failure must not become recovery");
      sn_notebook_free(notebook);
    } else {
      SN_CHECK(notebook == &sentinel, "failure leaves output alone");
      SN_CHECK(error.status == status && error.message[0],
               "failure has diagnostic");
    }
    if (!injected) break;
  }
  sn_test_alloc_fail_after(SIZE_MAX);
}
static void every_allocation_leaves_output_untouched(const char* fixture) {
  char path[1024];
  sn_test_fixture_path(path, sizeof path, fixture);
  size_t size;
  uint8_t* bytes = sn_test_read_file(path, &size);
  SN_CHECK(bytes != nullptr, "read allocation fixture");
  if (!bytes) return;
  check_allocations(bytes, size);
  free(bytes);
}

int main(void) {
  every_allocation_leaves_output_untouched(
      "supernote-typescript/blank-a6x-3.26.40-two-pages.note");
  every_allocation_leaves_output_untouched(
      "supernote-typescript/heading-n5-20260016-backgrounds-marker.note");
  const char* fixtures[] = {
      "samples/pdf-highlights-n6-20260016.mark",
      "samples/textbox-links-n6-20260016.note",
      "samples/picture-links-cover-n6-20260016.note",
      "supernote-typescript/rtr-n5-20230015-recognition.note",
      "supernote-typescript/blank-a6x-3.15.27-shapes-rtr.note"};
  for (size_t index = 0; index < sizeof fixtures / sizeof *fixtures; index++)
    every_allocation_leaves_output_untouched(fixtures[index]);
  for (size_t index = 0; index < 7; index++) {
    generated seed = gen_seed(index);
    check_allocations(seed.data, seed.size);
    free(seed.data);
  }
  return sn_test_report("allocation");
}
