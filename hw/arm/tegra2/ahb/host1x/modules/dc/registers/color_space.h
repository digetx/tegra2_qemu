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

#ifndef TEGRA_WINC_COLOR_SPACE_H
#define TEGRA_WINC_COLOR_SPACE_H

#include "regs.h"

#define WINC_CSC_YOF_OFFSET 0x611
#define WINC_CSC_YOF_RESET  0x00000000
typedef union winc_csc_yof_u {
    struct {
        unsigned int csc_yof:8;             /* Y Offset in s.7.0 format */
        unsigned int undefined_bits_8_31:24;
    };

    uint32_t reg32;
} winc_csc_yof_t;

#define WINC_CSC_KYRGB_OFFSET 0x612
#define WINC_CSC_KYRGB_RESET  0x00000000
typedef union winc_csc_kyrgb_u {
    struct {
        unsigned int csc_kyrgb:10;          /* Y Gain for R, G, B colors in 2.8 format */
        unsigned int undefined_bits_10_31:22;
    };

    uint32_t reg32;
} winc_csc_kyrgb_t;

#define WINC_CSC_KUR_OFFSET 0x613
#define WINC_CSC_KUR_RESET  0x00000000
typedef union winc_csc_kur_u {
    struct {
        unsigned int csc_kur:11;            /* U coefficients for R in s.2.8 format */
        unsigned int undefined_bits_11_31:21;
    };

    uint32_t reg32;
} winc_csc_kur_t;

#define WINC_CSC_KVR_OFFSET 0x614
#define WINC_CSC_KVR_RESET  0x00000000
typedef union winc_csc_kvr_u {
    struct {
        unsigned int csc_kvr:11;            /* V coefficients for R in s.2.8 format */
        unsigned int undefined_bits_11_31:21;
    };

    uint32_t reg32;
} winc_csc_kvr_t;

#define WINC_CSC_KUG_OFFSET 0x615
#define WINC_CSC_KUG_RESET  0x00000000
typedef union winc_csc_kug_u {
    struct {
        unsigned int csc_kug:10;            /* U coefficients for G in s.1.8 format */
        unsigned int undefined_bits_10_31:22;
    };

    uint32_t reg32;
} winc_csc_kug_t;

#define WINC_CSC_KVG_OFFSET 0x616
#define WINC_CSC_KVG_RESET  0x00000000
typedef union winc_csc_kvg_u {
    struct {
        unsigned int csc_kvg:10;            /* V coefficients for G in s.1.8 format */
        unsigned int undefined_bits_10_31:22;
    };

    uint32_t reg32;
} winc_csc_kvg_t;

#define WINC_CSC_KUB_OFFSET 0x617
#define WINC_CSC_KUB_RESET  0x00000000
typedef union winc_csc_kub_u {
    struct {
        unsigned int csc_kub:11;            /* U coefficients for B in s.2.8 format */
        unsigned int undefined_bits_11_31:21;
    };

    uint32_t reg32;
} winc_csc_kub_t;

#define WINC_CSC_KVB_OFFSET 0x618
#define WINC_CSC_KVB_RESET  0x00000000
typedef union winc_csc_kvb_u {
    struct {
        unsigned int csc_kvb:11;            /* V coefficients for B in s.2.8 format */
        unsigned int undefined_bits_11_31:21;
    };

    uint32_t reg32;
} winc_csc_kvb_t;

typedef struct csc_regs {
    DEFINE_REG32(winc_csc_yof);
    DEFINE_REG32(winc_csc_kyrgb);
    DEFINE_REG32(winc_csc_kur);
    DEFINE_REG32(winc_csc_kvr);
    DEFINE_REG32(winc_csc_kug);
    DEFINE_REG32(winc_csc_kvg);
    DEFINE_REG32(winc_csc_kub);
    DEFINE_REG32(winc_csc_kvb);
} csc;

extern regs_io_handler csc_handler;

#endif // TEGRA_WINC_COLOR_SPACE_H
