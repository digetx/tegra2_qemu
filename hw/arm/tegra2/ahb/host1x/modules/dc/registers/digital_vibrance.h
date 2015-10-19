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

#ifndef TEGRA_WINC_DIGITAL_VIRBANCE_H
#define TEGRA_WINC_DIGITAL_VIRBANCE_H

#include "regs.h"

#define WIN_DV_CONTROL_OFFSET 0x70E
#define WIN_B_DV_CONTROL_RESET  0x00000000
typedef union win_dv_control_u {
    struct {
        unsigned int dv_control_r:3;        /* Digital Vibrance control for R */
        unsigned int undefined_bits_3_7:5;
        unsigned int dv_control_g:3;        /* Digital Vibrance control for G */
        unsigned int undefined_bits_11_15:5;
        unsigned int dv_control_b:3;        /* Digital Vibrance control for B */
        unsigned int undefined_bits_19_31:13;
    };

    uint32_t reg32;
} win_dv_control_t;

typedef struct dv_regs {
    DEFINE_REG32(win_dv_control);
} dv;

extern regs_io_handler dv_handler;

#endif // TEGRA_WINC_DIGITAL_VIRBANCE_H
