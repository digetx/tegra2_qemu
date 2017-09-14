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

#ifndef TEGRA_WINC_VERTICAL_FILTERING_H
#define TEGRA_WINC_VERTICAL_FILTERING_H

#include "regs.h"

#define V_FILTER_SZ    16

#define WINC_V_FILTER_P00_OFFSET 0x619
#define WINC_V_FILTER_P00_RESET  0x00000000
typedef union winc_v_filter_p_u {
    struct {
        unsigned int v_filter_pc0:8;      /* Phase coefficient 0 (typically 128) */
        unsigned int undefined_bits_8_31:24;
    };

    uint32_t reg32;
} winc_v_filter_p_t;

typedef struct v_filter_regs {
    DEFINE_REG32(winc_v_filter_p)[V_FILTER_SZ];
} v_filter;

extern regs_io_handler v_filter_handler;

#endif // TEGRA_WINC_VERTICAL_FILTERING_H
