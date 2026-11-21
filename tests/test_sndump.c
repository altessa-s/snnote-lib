/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

/* Exercise the tool's actual private comparator without making it API. */
#define main sndump_main
#include "../tools/sndump.c"
#undef main
#include "sn_test.h"

#ifndef _WIN32
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sn_generate.h"
static void failed_flush(void) {
  char path[] = "/tmp/snnote-output-XXXXXX";
  int fd = mkstemp(path);
  SN_CHECK(fd >= 0, "synthetic input file");
  if (fd < 0) return;
  generated file = gen_begin(), footer = {0};
  gen_finish(&file, &footer);
  SN_CHECK(write(fd, file.data, file.size) == (ssize_t)file.size,
           "write synthetic input");
  close(fd);
  free(file.data);
  fflush(stdout);
  pid_t child = fork();
  SN_CHECK(child >= 0, "flush test child");
  if (child == 0) {
    static char buffer[65536];
    int readonly = open("/dev/null", O_RDONLY);
    if (readonly < 0 || dup2(readonly, STDOUT_FILENO) < 0) _exit(99);
    close(readonly);
    if (setvbuf(stdout, buffer, _IOFBF, sizeof buffer) != 0) _exit(99);
    char* args[] = {"sndump", path, nullptr};
    _exit(sndump_main(2, args));
  }
  if (child > 0) {
    int status = 0;
    SN_CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
                 WEXITSTATUS(status) == 2,
             "final buffered write failure returns 2");
  }
  unlink(path);
}
#endif

int main(void) {
#ifndef _WIN32
  failed_flush();
#endif
  sn_template names[] = {
      {.name = {"", 0}},      {.name = {"a", 1}},    {.name = {"a\0", 2}},
      {.name = {"a\0a", 3}},  {.name = {"a\0b", 3}}, {.name = {"a\x80", 2}},
      {.name = {"a\xff", 2}}, {.name = {"aa", 2}},   {.name = {"b", 1}},
  };
  /* Unsigned byte order puts high bytes after ASCII continuations. */
  sn_template* ordered[] = {&names[0], &names[1], &names[2],
                            &names[3], &names[4], &names[7],
                            &names[5], &names[6], &names[8]};
  constexpr size_t count = sizeof ordered / sizeof *ordered;
  for (size_t a = 0; a < count; a++)
    for (size_t b = 0; b < count; b++) {
      int order = compare_templates(&ordered[a], &ordered[b]);
      SN_CHECK((order > 0) - (order < 0) == (a > b) - (a < b),
               "bytewise template order %zu / %zu", a, b);
    }
  for (size_t shift = 0; shift < count; shift++) {
    sn_template* shuffled[count];
    for (size_t index = 0; index < count; index++)
      shuffled[index] = ordered[(count - 1 - index + shift) % count];
    qsort(shuffled, count, sizeof *shuffled, compare_templates);
    for (size_t index = 0; index < count; index++)
      SN_CHECK(shuffled[index] == ordered[index], "sorted template %zu", index);
  }
  sn_template equal = {.name = {"a\0a", 3}};
  sn_template* copy = &equal;
  SN_CHECK(compare_templates(&copy, &ordered[3]) == 0,
           "equal byte strings at distinct addresses");
  return sn_test_report("sndump");
}
