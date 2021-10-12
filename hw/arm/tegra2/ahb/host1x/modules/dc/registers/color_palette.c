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

#include "color_palette.h"

#define WINC_COLOR_PALETTE_OFFSET_END \
                    (WINC_COLOR_PALETTE_OFFSET + COLOR_PALETTE_SZ - 1)

static uint32_t pallette_read(void *regs, uint32_t offset)
{
    return 0;
}

static void pallette_write(void *regs, uint32_t offset, uint32_t value)
{
    color_palette *palette = regs;

    switch (offset) {
    case WINC_COLOR_PALETTE_OFFSET ... WINC_COLOR_PALETTE_OFFSET_END:
        palette->winc_color_palette[offset & 0xff].reg32 = value;
        break;
    case WINC_PALETTE_COLOR_EXT_OFFSET:
        palette->winc_palette_color_ext.reg32 = value;
        break;
    default:
        g_assert_not_reached();
    }
}

static void pallette_reset(void *regs)
{
    color_palette *palette = regs;
    unsigned i;

    for (i = 0; i < COLOR_PALETTE_SZ; i++) {
        palette->winc_color_palette[i].reg32 = WINC_COLOR_PALETTE_RESET;
    }

    palette->winc_palette_color_ext.reg32 = WINC_PALETTE_COLOR_EXT_RESET;
}

regs_io_handler pallette_handler = {
    .read  = pallette_read,
    .write = pallette_write,
    .reset = pallette_reset,
    .begin = 0x500,
    .end   = 0x600,
};
