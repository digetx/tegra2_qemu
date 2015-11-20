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

#ifndef TEGRA_WINC_COLOR_PALETTE_H
#define TEGRA_WINC_COLOR_PALETTE_H

#include "regs.h"

#define COLOR_PALETTE_SZ    256

#define WINC_COLOR_PALETTE_OFFSET 0x500
#define WINC_COLOR_PALETTE_RESET  0x00000000
typedef union winc_color_palette_u {
    struct {
        unsigned int color_palette_r:8;     /* Red Color Palette */
        unsigned int color_palette_g:8;     /* Green Color Palette */
        unsigned int color_palette_b:8;     /* Blue Color Palette */
        unsigned int undefined_bits_24_31:8;
    };

    uint32_t reg32;
} winc_color_palette_t;

#define WINC_PALETTE_COLOR_EXT_OFFSET 0x600
#define WINC_PALETTE_COLOR_EXT_RESET  0x00000000
typedef union winc_palette_color_ext_u {
    struct {
        unsigned int undefined_bit_0:1;
        unsigned int palette_color_ext:7;   /* Window A Palette Color Extension bits 7-1 are used for 1-bpp mode bits 7-2 are used for 2-bpp mode bits 7-4 are used for 4-bpp mode */
        unsigned int undefined_bits_8_31:24;
    };

    uint32_t reg32;
} winc_palette_color_ext_t;

typedef struct color_palette_regs {
    DEFINE_REG32(winc_color_palette)[COLOR_PALETTE_SZ];
    DEFINE_REG32(winc_palette_color_ext);
} color_palette;

extern regs_io_handler pallette_handler;

#endif // TEGRA_WINC_COLOR_PALETTE_H
