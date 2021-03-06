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

/* Autogenerated from TRM v02p */

#ifndef TEGRA_TIMER_US_H
#define TEGRA_TIMER_US_H

#define CNTR_1US_OFFSET 0x0
#define CNTR_1US_RESET  0x00000000
typedef union cntr_1us_u {
    struct {
        unsigned int low_value:16;          /* Elapsed time in micro-second */
        unsigned int high_value:16;         /* Elapsed time in micro-second */
    };

    uint32_t reg32;
} cntr_1us_t;

#define USEC_CFG_OFFSET 0x4
#define USEC_CFG_RESET  0x0000000C
typedef union usec_cfg_u {
    struct {
        unsigned int usec_divisor:8;        /* usec divisor */
        unsigned int usec_dividend:8;       /* usec dividend */
        unsigned int undefined_bits_16_31:16;
    };

    uint32_t reg32;
} usec_cfg_t;

#define CNTR_FREEZE_OFFSET 0x3C
#define CNTR_FREEZE_RESET  0x00000000
typedef union cntr_freeze_u {
    struct {
        unsigned int dbg_freeze_cpu0:1;     /* 1 = freeze timers when CPU0 is in debug state, 0 = no freeze */
        unsigned int dbg_freeze_cpu1:1;     /* 1 = freeze timers when CPU1 is in debug state, 0 = no freeze */
        unsigned int undefined_bits_2_3:2;
        unsigned int dbg_freeze_cop:1;      /* 1 = freeze timers when COP is in debug state, 0 = no freeze */
        unsigned int undefined_bits_5_31:27;
    };

    uint32_t reg32;
} cntr_freeze_t;

#define DEFINE_REG32(reg) reg##_t reg

typedef struct tegra_timer_us_state {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    ptimer_state *ptimer;
    DEFINE_REG32(cntr_1us);
    DEFINE_REG32(usec_cfg);
    DEFINE_REG32(cntr_freeze);
} tegra_timer_us;

#endif // TEGRA_TIMER_US_H
