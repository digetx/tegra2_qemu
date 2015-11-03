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

#include <glib.h>

#include "horizontal_filtering.h"

#define WINC_H_FILTER_OFFSET_END    (WINC_H_FILTER_P00_OFFSET + H_FILTER_SZ - 1)

static uint32_t h_filter_read(void *regs, uint32_t offset)
{
    h_filter *hf = regs;
    uint32_t ret = 0;

    switch (offset) {
    case WINC_H_FILTER_P00_OFFSET ... WINC_H_FILTER_OFFSET_END:
        ret = hf->winc_h_filter_p[(offset - 1) & 0xf].reg32;
        break;
    default:
        g_assert_not_reached();
    }

    return ret;
}

static void h_filter_write(void *regs, uint32_t offset, uint32_t value)
{
    h_filter *hf = regs;

    switch (offset) {
    case WINC_H_FILTER_P00_OFFSET ... WINC_H_FILTER_OFFSET_END:
        hf->winc_h_filter_p[(offset - 1) & 0xf].reg32 = value;
        break;
    default:
        g_assert_not_reached();
    }
}

static void h_filter_reset(void *regs)
{
    h_filter *hf = regs;
    unsigned i;

    for (i = 0; i < H_FILTER_SZ; i++) {
        hf->winc_h_filter_p[i].reg32 = WINC_H_FILTER_P00_RESET;
    }
}

regs_io_handler h_filter_handler = {
    .read  = h_filter_read,
    .write = h_filter_write,
    .reset = h_filter_reset,
    .begin = 0x601,
    .end   = 0x610,
};
