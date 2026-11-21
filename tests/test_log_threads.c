/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include <stdatomic.h>

#include "sn_internal.h"
#include "sn_test.h"
#if defined(SN_HAVE_C_THREADS)
#include <threads.h>
typedef thrd_t test_thread;
#elif defined(SN_HAVE_PTHREADS)
#include <pthread.h>
typedef pthread_t test_thread;
#endif

#if defined(SN_HAVE_C_THREADS) || defined(SN_HAVE_PTHREADS)
static atomic_uint mismatches, callbacks;
static atomic_bool start_workers, cancel_workers;
static int first_context, second_context, third_context;

/* Distinct callbacks expose torn callback/context publication. Contexts
 * remain alive until all parser threads have joined. */
static void first_sink(sn_log_level level, const char* message, void* context) {
  atomic_fetch_add(&callbacks, 1);
  (void)level;
  (void)message;
  if (context != &first_context) atomic_fetch_add(&mismatches, 1);
}

static void second_sink(sn_log_level level, const char* message,
                        void* context) {
  atomic_fetch_add(&callbacks, 1);
  (void)level;
  (void)message;
  if (context != &second_context) atomic_fetch_add(&mismatches, 1);
}

static void shared_sink(sn_log_level level, const char* message,
                        void* context) {
  atomic_fetch_add(&callbacks, 1);
  (void)level;
  (void)message;
  if (context != &first_context && context != &third_context)
    atomic_fetch_add(&mismatches, 1);
}

/* Run two writers and two parsers concurrently; the parser's malformed
 * optional block exercises the same diagnostic path used by notebooks. */
static void exercise(bool install) {
  while (!atomic_load(&start_workers)) {
  }
  if (atomic_load(&cancel_workers)) return;
  /* Let a parser reach the initial binding before replacements can silence it.
   */
  if (install)
    while (!atomic_load(&callbacks)) {
    }
  for (size_t iteration = 0; iteration < 2000; iteration++) {
    if (install) {
      sn_log_install(first_sink, &first_context);
      sn_log_install(first_sink, &first_context);
      sn_log_install(second_sink, &second_context);
      sn_log_install(shared_sink, &first_context);
      sn_log_install(shared_sink, &third_context);
      sn_log_install(first_sink, &first_context);
      sn_log_silence();
    } else {
      sn_arena arena = {0};
      sn_page page = {0};
      const uint8_t truncated[] = {1};
      sn_strokes(&arena, (sn_bytes){truncated, sizeof truncated}, &page);
      sn_log_writef(SN_LOG_DEBUG, "concurrent binding reuse");
      sn_arena_free(&arena);
    }
  }
}

#if defined(SN_HAVE_C_THREADS)
static int worker(void* context) {
  exercise(context != nullptr);
  return 0;
}
#else
static void* worker(void* context) {
  exercise(context != nullptr);
  return nullptr;
}
#endif
#endif

int main(void) {
#if defined(SN_HAVE_C_THREADS) || defined(SN_HAVE_PTHREADS)
  test_thread threads[4];
  bool started[4] = {false};
  for (size_t index = 0; index < 4; index++) {
    void* context = index < 2 ? &first_context : nullptr;
#if defined(SN_HAVE_C_THREADS)
    started[index] =
        thrd_create(threads + index, worker, context) == thrd_success;
#else
    started[index] =
        pthread_create(threads + index, nullptr, worker, context) == 0;
#endif
    SN_CHECK(started[index], "start thread");
  }
  bool all_started = true;
  for (size_t index = 0; index < 4; index++) all_started &= started[index];
  sn_log_install(first_sink, &first_context);
  atomic_store(&cancel_workers, !all_started);
  atomic_store(&start_workers, true);
  for (size_t index = 0; index < 4; index++) {
    if (!started[index]) continue;
#if defined(SN_HAVE_C_THREADS)
    thrd_join(threads[index], nullptr);
#else
    pthread_join(threads[index], nullptr);
#endif
  }
  SN_CHECK(atomic_load(&callbacks) > 0, "callbacks executed");
  sn_log_silence();
  SN_CHECK(atomic_load(&mismatches) == 0,
           "callback/context pairs remain intact");
#else
  puts("SKIP: neither C threads nor pthreads available");
#endif
  return sn_test_report("log_threads");
}
