/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

/* Regression-only page composition. Not an installed library interface. */
#ifndef SN_COMPOSITION_H
#define SN_COMPOSITION_H
#include "snnote/snnote.h"
[[nodiscard]] sn_status sn_page_raster(const sn_page* page,
                                       const sn_palette* palette,
                                       bool include_background, sn_raster** out,
                                       sn_error* err);
[[nodiscard]] sn_status sn_raster_over(const sn_raster* above,
                                       const sn_raster* below, sn_raster** out,
                                       sn_error* err);
[[nodiscard]] sn_status sn_raster_flatten(const sn_raster* raster,
                                          uint8_t paper, sn_raster** out,
                                          sn_error* err);
#endif
