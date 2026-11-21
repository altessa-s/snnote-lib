/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_internal.h"
#include "sn_test.h"
static int lines, errors;
static void sink(sn_log_level level, const char* message, void* context) {
  SN_CHECK(context == &lines, "context pair");
  SN_CHECK(!strstr(message, "private"), "no content");
  lines++;
  if (level == SN_LOG_ERROR) errors++;
}
static int pair_lines[2];
static void pair_sink(sn_log_level level, const char* message, void* context) {
  (void)level;
  SN_CHECK(strcmp(message, "binding reuse") == 0, "whole line delivered");
  SN_CHECK(context == &pair_lines[0] || context == &pair_lines[1],
           "retained context");
  if (context == &pair_lines[0] || context == &pair_lines[1])
    (*(int*)context)++;
}
static void other_sink(sn_log_level level, const char* message, void* context) {
  SN_CHECK(context == &pair_lines[0], "distinct callback paired with context");
  pair_sink(level, message, context);
}
static void binding_reuse(void) {
  size_t before = sn_test_log_allocations();
  sn_log_install(pair_sink, &pair_lines[0]);
  SN_CHECK(sn_test_log_allocations() == before + 1, "first pair allocates");
  for (size_t iteration = 0; iteration < 100000; iteration++) {
    sn_log_silence();
    sn_log_install(pair_sink, &pair_lines[0]);
    sn_log_writef(SN_LOG_DEBUG, "binding reuse");
  }
  SN_CHECK(sn_test_log_allocations() == before + 1,
           "100000 reinstalls allocate nothing");
  SN_CHECK(pair_lines[0] == 100000, "every reinstall delivers a line");
  sn_log_install(pair_sink, &pair_lines[1]);
  sn_log_writef(SN_LOG_DEBUG, "binding reuse");
  SN_CHECK(pair_lines[1] == 1 && pair_lines[0] == 100000,
           "same sink with distinct context takes effect");
  sn_log_install(other_sink, &pair_lines[0]);
  sn_log_writef(SN_LOG_DEBUG, "binding reuse");
  SN_CHECK(sn_test_log_allocations() == before + 3,
           "each distinct pair allocates");
  sn_log_install(pair_sink, &pair_lines[0]);
  sn_log_writef(SN_LOG_DEBUG, "binding reuse");
  SN_CHECK(pair_lines[0] == 100002 && pair_lines[1] == 1 &&
               sn_test_log_allocations() == before + 3,
           "A B A reuses the older entry and delivers to A");
  sn_log_install(nullptr, &pair_lines[0]);
  SN_CHECK(!sn_log_wanted() && sn_test_log_allocations() == before + 3,
           "null install silences without allocation");
}
static void formatted_sink(sn_log_level level, const char* message,
                           void* context) {
  (void)level;
  SN_CHECK(strcmp(message, context) == 0,
           "formatted diagnostic delivered whole");
}
static void formatted_lines(void) {
  static char expected[1024];
  memset(expected, 'x', sizeof expected - 1);
  sn_log_install(formatted_sink, expected);
  sn_log_writef(SN_LOG_DEBUG, "%s", expected);
  strcpy(expected, "count 42");
  sn_log_writef(SN_LOG_DEBUG, "count %zu", (size_t)42);
  sn_log_silence();
}

int main(void) {
  sn_log_silence();
  SN_CHECK(!sn_log_wanted(), "silent");
  sn_log_install(sink, &lines);
  sn_arena arena = {0};
  sn_page page = {0};
  const uint8_t bad[] = {1};
  sn_strokes(&arena, (sn_bytes){bad, 1}, &page);
  SN_CHECK(lines == 1 && errors == 1 && page.skipped_strokes == 1,
           "lost record logged");
  const uint8_t content[] = "private";
  (void)sn_recognition_read(&arena, (sn_bytes){content, sizeof content - 1});
  SN_CHECK(errors == 2, "recognition loss logged");
  sn_log_silence();
  sn_log_writef(SN_LOG_ERROR, "not emitted");
  SN_CHECK(lines == 2, "silenced");
  sn_arena_free(&arena);
  binding_reuse();
  formatted_lines();
  return sn_test_report("log");
}
