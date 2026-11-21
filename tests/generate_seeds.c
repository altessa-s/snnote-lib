/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */
#include "sn_generate.h"
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  for (size_t index = 0; index < 7; index++) {
    generated seed = gen_seed(index);
    sn_notebook* notebook = nullptr;
    if (sn_parse_notebook(seed.data, seed.size, &notebook, nullptr) != SN_OK)
      return 1;
    if (notebook->page_count != 1) return 1;
    if (index < 2 && notebook->pages[0].stroke_count != 1) return 1;
    if (index == 2 && notebook->title_count != 1) return 1;
    if (index == 3 && notebook->keyword_count != 1) return 1;
    if (index == 4 && notebook->link_count != 1) return 1;
    if (index == 5 && notebook->highlight_count != 1) return 1;
    if (index == 6 && !notebook->pages[0].recognition) return 1;
    sn_notebook_free(notebook);
    char path[1024];
    snprintf(path, sizeof path, "%s/seed-%zu.%s", argv[1], index,
             index == 5 ? "mark" : "note");
    FILE* file = fopen(path, "wb");
    if (!file) return 2;
    bool ok = fwrite(seed.data, 1, seed.size, file) == seed.size;
    if (fclose(file) || !ok) return 2;
    free(seed.data);
  }
  return 0;
}
