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

#ifndef TEGRA_WINC_HORIZONTAL_FILTERING_H
#define TEGRA_WINC_HORIZONTAL_FILTERING_H

#include "regs.h"

#define H_FILTER_SZ    16

#define WINC_H_FILTER_P00_OFFSET 0x601
#define WINC_H_FILTER_P00_RESET  0x00000000
typedef union winc_h_filter_p_u {
    struct {
        unsigned int h_filter_pc0:3;      /* Phase coefficient 0 */
        unsigned int h_filter_pc1:5;      /* Phase coefficient 1 */
        unsigned int h_filter_pc2:8;      /* Phase coefficient 2 */
        unsigned int h_filter_pc3:8;      /* Phase coefficient 3 */
        unsigned int h_filter_pc4:5;      /* Phase coefficient 4 */
        unsigned int h_filter_pc5:3;      /* Phase coefficient 5 */
    };

    uint32_t reg32;
} winc_h_filter_p_t;

typedef struct h_filter_regs {
    DEFINE_REG32(winc_h_filter_p)[H_FILTER_SZ];
} h_filter;

extern regs_io_handler h_filter_handler;

#endif // TEGRA_WINC_HORIZONTAL_FILTERING_H
