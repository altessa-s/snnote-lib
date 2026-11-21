/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */
#include <snnote/snnote.h>

#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  FILE* file = std::fopen(argv[1], "rb");
  if (!file) return 2;
  std::fseek(file, 0, SEEK_END);
  long length = std::ftell(file);
  std::rewind(file);
  if (length < 0) return 2;
  std::vector<uint8_t> data(static_cast<size_t>(length));
  size_t read = std::fread(data.data(), 1, data.size(), file);
  std::fclose(file);
  if (read != data.size()) return 2;
  sn_notebook* notebook = nullptr;
  if (sn_parse_notebook(data.data(), data.size(), &notebook, nullptr) != SN_OK)
    return 1;
  size_t points = 0;
  for (size_t page = 0; page < notebook->page_count; page++) {
    const sn_page& value = notebook->pages[page];
    for (size_t stroke = 0; stroke < value.stroke_count; stroke++) {
      points += value.strokes[stroke].point_count;
      for (size_t point = 0; point < value.strokes[stroke].point_count; point++)
        if (value.strokes[stroke].points[point].position.x !=
            value.strokes[stroke].points[point].position.x)
          return 1;
    }
  }
  std::printf("%zu pages, %zu points\n", notebook->page_count, points);
  sn_notebook_free(notebook);
  return 0;
}
