/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_fixture.h"

/* Whole-file diagnostics identify format locations without copying input. */
static void expect_error(const buffer* file, size_t size, sn_status expected,
                         const char* location, int64_t value) {
  sn_notebook sentinel = {0};
  sn_notebook* output = &sentinel;
  sn_error error = {0};
  sn_status status = sn_parse_notebook(file->data, size, &output, &error);
  SN_CHECK(status == expected && error.status == expected, "whole-file status");
  SN_CHECK(output == &sentinel, "failed parse leaves output alone");
  SN_CHECK(strstr(error.message, location) != nullptr, "format location: %s",
           error.message);
  SN_CHECK(error.value == value, "numeric location");
}

int main(void) {
  buffer file = notebook(nullptr);
  expect_error(&file, 0, SN_ERR_UNSUPPORTED_FORMAT, "signature", 0);
  file.data[16] = 'x';
  expect_error(&file, file.size, SN_ERR_UNSUPPORTED_FORMAT, "version", 16);
  file = notebook(nullptr);
  expect_error(&file, 24, SN_ERR_UNEXPECTED_EOF, "footer address", 24);
  u32at(&file, file.size - 4, UINT32_MAX);
  expect_error(&file, file.size, SN_ERR_MALFORMED, "footer block", UINT32_MAX);
  file = begin();
  finish(&file, "<FILE_FEATURE:-1>");
  expect_error(&file, file.size, SN_ERR_MALFORMED, "FILE_FEATURE", -1);
  file = begin();
  size_t header = tags(&file, "<APPLY_EQUIPMENT:private-content>");
  char footer[128];
  snprintf(footer, sizeof footer, "<FILE_FEATURE:%zu>", header);
  finish(&file, footer);
  expect_error(&file, file.size, SN_ERR_MALFORMED, "APPLY_EQUIPMENT", 0);
  sn_error error;
  sn_notebook* output = nullptr;
  sn_status status = sn_parse_notebook(nullptr, 0, &output, &error);
  SN_CHECK(
      status == SN_ERR_INVALID_ARGUMENT && strstr(error.message, "pointer"),
      "null argument diagnostic");
  return sn_test_report("errors");
}
