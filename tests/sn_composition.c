/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "sn_composition.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static sn_status sn_error_set(sn_error* error, sn_status status,
                              int64_t value) {
  if (error) *error = (sn_error){.status = status, .value = value};
  return status;
}

static sn_status dimensions(size_t width, size_t height, size_t* count) {
  if (!width || !height) return SN_ERR_INVALID_ARGUMENT;
  if (width > SN_MAX_RASTER_PIXELS / height) return SN_ERR_LIMIT;
  *count = width * height;
  return SN_OK;
}
static sn_raster* raster(size_t width, size_t height, size_t pixel_count) {
  sn_raster* result = malloc(sizeof *result + pixel_count * 2);
  if (!result) return nullptr;
  uint8_t* source_page = (uint8_t*)(result + 1);
  *result = (sn_raster){width, height, source_page, source_page + pixel_count};
  return result;
}
static sn_status valid(const sn_raster* result, size_t* pixel_count) {
  if (!result) return SN_ERR_INVALID_ARGUMENT;
  if (!result->width || !result->height) {
    *pixel_count = 0;
    return SN_OK;
  }
  if (!result->gray || !result->alpha) return SN_ERR_INVALID_ARGUMENT;
  return dimensions(result->width, result->height, pixel_count);
}
sn_status sn_raster_over(const sn_raster* above, const sn_raster* below,
                         sn_raster** out, sn_error* err) {
  size_t pixel_count, below_count;
  sn_status status = valid(above, &pixel_count);
  if (status == SN_OK) status = valid(below, &below_count);
  if (!out) status = SN_ERR_INVALID_ARGUMENT;
  if (status != SN_OK) return sn_error_set(err, status, 0);
  if (above->width != below->width || above->height != below->height)
    return sn_error_set(err, SN_ERR_MALFORMED, 0);
  sn_raster* result = raster(above->width, above->height, pixel_count);
  if (!result) return sn_error_set(err, SN_ERR_OUT_OF_MEMORY, 0);
  uint8_t* gray = (uint8_t*)(result + 1);
  uint8_t* alpha = gray + pixel_count;
  for (size_t index = 0; index < pixel_count; index++) {
    unsigned above_alpha = above->alpha[index],
             below_alpha = below->alpha[index];
    gray[index] = below->gray[index];
    alpha[index] = (uint8_t)below_alpha;
    if (above_alpha == 255) {
      gray[index] = above->gray[index];
      alpha[index] = 255;
    } else if (above_alpha) {
      unsigned covered = above_alpha + below_alpha * (255 - above_alpha) / 255;
      unsigned mixed = ((unsigned)above->gray[index] * above_alpha +
                        (unsigned)below->gray[index] * below_alpha *
                            (255 - above_alpha) / 255) /
                       covered;
      gray[index] = (uint8_t)mixed;
      alpha[index] = (uint8_t)covered;
    }
  }
  *out = result;
  return sn_error_set(err, SN_OK, 0);
}
sn_status sn_raster_flatten(const sn_raster* input, uint8_t paper,
                            sn_raster** out, sn_error* err) {
  size_t pixel_count;
  sn_status status = valid(input, &pixel_count);
  if (!out) status = SN_ERR_INVALID_ARGUMENT;
  if (status != SN_OK) return sn_error_set(err, status, 0);
  sn_raster* result = raster(input->width, input->height, pixel_count);
  if (!result) return sn_error_set(err, SN_ERR_OUT_OF_MEMORY, 0);
  uint8_t* gray = (uint8_t*)(result + 1);
  uint8_t* alpha = gray + pixel_count;
  for (size_t index = 0; index < pixel_count; index++) {
    unsigned above_alpha = input->alpha[index];
    gray[index] = (uint8_t)(((unsigned)input->gray[index] * above_alpha +
                             (unsigned)paper * (255 - above_alpha)) /
                            255);
    alpha[index] = 255;
  }
  *out = result;
  return sn_error_set(err, SN_OK, 0);
}
sn_status sn_page_raster(const sn_page* source_page, const sn_palette* palette,
                         bool background, sn_raster** out, sn_error* err) {
  if (!source_page || !out || !isfinite(source_page->size.width) ||
      !isfinite(source_page->size.height) || source_page->size.width < 1 ||
      source_page->size.height < 1 ||
      source_page->size.width > SN_MAX_RASTER_PIXELS ||
      source_page->size.height > SN_MAX_RASTER_PIXELS ||
      (source_page->layer_count && !source_page->layers))
    return sn_error_set(err, SN_ERR_INVALID_ARGUMENT, 0);
  size_t width = (size_t)source_page->size.width,
         height = (size_t)source_page->size.height, pixel_count;
  sn_status status = dimensions(width, height, &pixel_count);
  if (status != SN_OK) return sn_error_set(err, status, 0);
  sn_raster* page = raster(width, height, pixel_count);
  if (!page) return sn_error_set(err, SN_ERR_OUT_OF_MEMORY, 0);
  memset((uint8_t*)(page + 1), 255, pixel_count);
  memset((uint8_t*)(page + 1) + pixel_count, 0, pixel_count);
  for (size_t index = source_page->layer_count; index > 0; index--) {
    const sn_layer* layer = source_page->layers + index - 1;
    if (!layer->is_visible || !layer->bitmap ||
        (!background && layer->name == SN_LAYER_BACKGROUND))
      continue;
    sn_raster *top = nullptr, *next = nullptr;
    status = sn_bitmap_raster(layer->bitmap, width, height, palette, &top, err);
    if (status == SN_OK) status = sn_raster_over(top, page, &next, err);
    sn_raster_free(top);
    if (status != SN_OK) {
      sn_raster_free(page);
      return status;
    }
    sn_raster_free(page);
    page = next;
  }
  sn_raster* result = nullptr;
  status = sn_raster_flatten(page, 255, &result, err);
  sn_raster_free(page);
  if (status == SN_OK) *out = result;
  return status;
}
