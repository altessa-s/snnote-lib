/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

/* The whole of the test harness: a counter, a macro that says what failed and
 * where, and a main that answers with the count. A library of four hundred
 * lines to run twenty assertions would be more machinery than the thing it
 * tests. */

#ifndef SN_TEST_H
#define SN_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t sn_test_failures = 0;
static size_t sn_test_checks = 0;

#define SN_CHECK(condition, ...)                                               \
  do {                                                                         \
    sn_test_checks += 1;                                                       \
    if (!(condition)) {                                                        \
      sn_test_failures += 1;                                                   \
      fprintf(stderr, "%s:%d: FAIL %s\n    ", __FILE__, __LINE__, #condition); \
      fprintf(stderr, __VA_ARGS__);                                            \
      fprintf(stderr, "\n");                                                   \
    }                                                                          \
  } while (0)

#define SN_CHECK_EQ_SIZE(actual, expected, ...)                            \
  do {                                                                     \
    size_t sn_a_ = (actual), sn_e_ = (expected);                           \
    sn_test_checks += 1;                                                   \
    if (sn_a_ != sn_e_) {                                                  \
      sn_test_failures += 1;                                               \
      fprintf(stderr, "%s:%d: FAIL %s == %s (%zu vs %zu)\n    ", __FILE__, \
              __LINE__, #actual, #expected, sn_a_, sn_e_);                 \
      fprintf(stderr, __VA_ARGS__);                                        \
      fprintf(stderr, "\n");                                               \
    }                                                                      \
  } while (0)

#define SN_CHECK_EQ_STR(actual, actual_len, expected, ...)              \
  do {                                                                  \
    const char* sn_a_ = (actual);                                       \
    size_t sn_al_ = (actual_len);                                       \
    const char* sn_e_ = (expected);                                     \
    sn_test_checks += 1;                                                \
    if (sn_al_ != strlen(sn_e_) || memcmp(sn_a_, sn_e_, sn_al_) != 0) { \
      sn_test_failures += 1;                                            \
      fprintf(stderr, "%s:%d: FAIL %s == \"%s\" (got \"%.*s\")\n    ",  \
              __FILE__, __LINE__, #actual, sn_e_, (int)sn_al_, sn_a_);  \
      fprintf(stderr, __VA_ARGS__);                                     \
      fprintf(stderr, "\n");                                            \
    }                                                                   \
  } while (0)

[[maybe_unused]] static int sn_test_report(const char* suite) {
  if (sn_test_failures == 0) {
    printf("%s: %zu checks, all passed\n", suite, sn_test_checks);
    return 0;
  }
  printf("%s: %zu checks, %zu FAILED\n", suite, sn_test_checks,
         sn_test_failures);
  return 1;
}

/* Read a whole file. The tests do their own I/O: the library takes bytes and
 * has never opened a file, which is what keeps it testable against a corpus
 * and nothing else. */
[[maybe_unused]] static unsigned char* sn_test_read_file(const char* path,
                                                         size_t* out_size) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) return NULL;
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }
  long size = ftell(file);
  if (size < 0) {
    fclose(file);
    return NULL;
  }
  rewind(file);
  unsigned char* bytes = malloc((size_t)size + 1u);
  if (bytes == NULL) {
    fclose(file);
    return NULL;
  }
  size_t read = fread(bytes, 1, (size_t)size, file);
  fclose(file);
  if (read != (size_t)size) {
    free(bytes);
    return NULL;
  }
  *out_size = read;
  return bytes;
}

#ifndef SN_FIXTURE_DIR
#define SN_FIXTURE_DIR "tests/fixtures"
#endif

[[maybe_unused]] static void sn_test_fixture_path(char* out, size_t size,
                                                  const char* name) {
  snprintf(out, size, "%s/%s", SN_FIXTURE_DIR, name);
}

#endif /* SN_TEST_H */
