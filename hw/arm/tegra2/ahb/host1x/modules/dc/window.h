/*
 * ARM NVIDIA Tegra2 emulation.
 *
 * Copyright (c) 2014-2015 Dmitry Osipenko <digetx@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TEGRA_DC_WIN_H
#define TEGRA_DC_WIN_H

#include "registers/win_common.h"

enum {
    CAP_COLOR_PALETTE = 0,
    CAP_DIGITAL_VIBRANCE,
    CAP_COLOR_SPACE_CONVERSION,
    CAP_HORIZONTAL_FILTERING,
    CAP_VERTICAL_FILTERING,
    CAPS_NB
};

#define CAP_COLOR_PALETTE_BIT           (1 << (CAP_COLOR_PALETTE))
#define CAP_DIGITAL_VIBRANCE_BIT        (1 << (CAP_DIGITAL_VIBRANCE))
#define CAP_COLOR_SPACE_CONVERSION_BIT  (1 << (CAP_COLOR_SPACE_CONVERSION))
#define CAP_HORIZONTAL_FILTERING_BIT    (1 << (CAP_HORIZONTAL_FILTERING))
#define CAP_VERTICAL_FILTERING_BIT      (1 << (CAP_VERTICAL_FILTERING))

struct win_regs;

typedef struct display_window {
    struct DisplaySurface *surface;
    QLIST_HEAD(, win_regs) regs_list;
    win_common_regs regs_active;
    win_common_regs regs_assembly;
    int caps;
} display_window;

void init_window(display_window *win, int caps);
void reset_window(display_window *win);
uint32_t read_window(display_window *win, uint32_t offset, int st);
void write_window(display_window *win, uint32_t offset, uint32_t value, int st);
void latch_window_assembly(display_window *win);

#endif // TEGRA_DC_WIN_H
