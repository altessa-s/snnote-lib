/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include <fenv.h>
#include <inttypes.h>
#include <locale.h>

#include "sn_internal.h"
#include "sn_test.h"

/* Expected values are compiled constants, independent of the active locale. */
static void decimal_values_are_locale_independent(void) {
  const struct {
    const char* text;
    double value;
  } cases[] = {{"0", 0},
               {"-0", -0.0},
               {".5", 0.5},
               {"1.25e2", 125},
               {"0.1", 0.1},
               {"1.2345678901234567", 1.2345678901234567},
               {"9007199254740993", 9007199254740992.0},
               {"2.2250738585072014e-308", 0x1p-1022},
               {"4.9406564584124654e-324", 0x1p-1074},
               {"1.7976931348623157e308", 0x1.fffffffffffffp1023},
               {"1e22", 1e22},
               {"9007199254740992e-22", 9007199254740992e-22},
               {"9999999999999997952e22", 9999999999999997952e22},
               {"1e400", INFINITY},
               {"-1e-400", -0.0}};
  for (size_t index = 0; index < sizeof cases / sizeof *cases; index++) {
    double value = 42;
    SN_CHECK(
        sn_double((sn_string){cases[index].text, strlen(cases[index].text)},
                  &value),
        "decimal syntax %zu", index);
    SN_CHECK(value == cases[index].value &&
                 signbit(value) == signbit(cases[index].value),
             "nearest binary64 value %zu", index);
  }
  const char* invalid[] = {"1,25", " 1",  "1e",       ".",     "1x",
                           "nan",  "inf", "infinity", "0x1p2", ""};
  for (size_t index = 0; index < sizeof invalid / sizeof *invalid; index++) {
    double value = 42;
    SN_CHECK(!sn_double((sn_string){invalid[index], strlen(invalid[index])},
                        &value) &&
                 value == 42,
             "bad syntax leaves output unchanged");
  }
}

static void generated_values(void) {
  uint64_t state = 1;
  for (size_t index = 0; index < 1000000; index++) {
    state = state * UINT64_C(6364136223846793005) + 1;
    uint64_t coefficient =
        index % 3 ? state % UINT64_C(9007199254740993)
                  : (state % UINT64_C(10000000000000000000)) & ~UINT64_C(2047);
    char text[80];
    int length =
        snprintf(text, sizeof text, "%s%" PRIu64 "e%d", index % 2 ? "-" : "",
                 coefficient, (int)(index % 45) - 22);
    double fast, slow;
    bool first = sn_double((sn_string){text, (size_t)length}, &fast);
    bool second =
        sn_test_double_fallback((sn_string){text, (size_t)length}, &slow);
    SN_CHECK(first && second && !memcmp(&fast, &slow, sizeof fast),
             "exact fast path %s", text);
  }
}

static void slow_values(void) {
  setlocale(LC_NUMERIC, "C");
  uint64_t state = 7;
  for (size_t index = 0; index < 1000000; index++) {
    state = state * UINT64_C(6364136223846793005) + 1;
    int exponent =
        index % 2 ? -23 - (int)(index % 318) : 23 + (int)(index % 258);
    char text[96];
    int length = snprintf(text, sizeof text, "%s%" PRIu64 "e%d",
                          index % 3 ? "" : "-", state, exponent);
    double actual, expected = strtod(text, nullptr);
    sn_test_decimal_reset();
    SN_CHECK(sn_double((sn_string){text, (size_t)length}, &actual) &&
                 !memcmp(&actual, &expected, sizeof actual),
             "slow path %s", text);
    SN_CHECK(sn_test_decimal_operations() <= 57, "bounded normalization %s",
             text);
  }
  const char* extremes[] = {"1e-300", "1e-324", "1e308", "1e-1000000",
                            "1e1000000"};
  for (size_t index = 0; index < sizeof extremes / sizeof *extremes; index++) {
    double value;
    sn_test_decimal_reset();
    SN_CHECK(sn_double((sn_string){extremes[index], strlen(extremes[index])},
                       &value) &&
                 sn_test_decimal_operations() <= 57,
             "extreme normalization");
  }
}

static void huge_tokens(void) {
  char* text = malloc(2000100);
  SN_CHECK(text != nullptr, "huge token storage");
  if (!text) return;
  text[0] = '1';
  memset(text + 1, '0', 1000800);
  strcpy(text + 1000801, "e-1000800");
  double value;
  SN_CHECK(sn_double((sn_string){text, strlen(text)}, &value) && value == 1,
           "dropped digits cancel explicit exponent exactly");
  strcpy(text, "0.");
  memset(text + 2, '0', 2000000);
  strcpy(text + 2000002, "1e1000000");
  SN_CHECK(sn_double((sn_string){text, strlen(text)}, &value) && value == 0,
           "fraction exponent does not saturate before cancellation");
  const char* extremes[] = {"1e999999999999999999999999",
                            "-1e-999999999999999999999999",
                            "0e999999999999999999999999"};
  for (size_t i = 0; i < 3; i++) {
    SN_CHECK(sn_double((sn_string){extremes[i], strlen(extremes[i])}, &value) &&
                 (i == 0 ? isinf(value) : value == 0),
             "huge explicit exponent");
  }
  free(text);
}

int main(void) {
  huge_tokens();
  generated_values();
  slow_values();
  int original_rounding = fegetround();
  if (fesetround(FE_DOWNWARD) == 0) {
    double fast, slow;
    sn_string text = {"0.1", 3};
    SN_CHECK(sn_double(text, &fast) && sn_test_double_fallback(text, &slow) &&
                 !memcmp(&fast, &slow, sizeof fast),
             "nondefault rounding uses exact fallback");
    (void)fesetround(original_rounding);
  }
  decimal_values_are_locale_independent();
  const char* locales[] = {"de_DE.UTF-8", "fr_FR.UTF-8", "ru_RU.UTF-8",
                           "de_DE"};
  bool exercised = false;
  for (size_t index = 0; index < sizeof locales / sizeof *locales; index++) {
    if (setlocale(LC_NUMERIC, locales[index]) &&
        strcmp(localeconv()->decimal_point, ",") == 0) {
      decimal_values_are_locale_independent();
      exercised = true;
      break;
    }
  }
  printf("comma-decimal locale: %s\n",
         exercised ? "tested" : "unavailable; value table tested");
  setlocale(LC_NUMERIC, "C");
  return sn_test_report("decimal");
}
