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

#include "digital_vibrance.h"

static uint32_t dv_read(void *regs, uint32_t offset)
{
    dv *dv = regs;

    switch (offset) {
    case WIN_DV_CONTROL_OFFSET:
        break;
    default:
        g_assert_not_reached();
    }

    return dv->win_dv_control.reg32;
}

static void dv_write(void *regs, uint32_t offset, uint32_t value)
{
    dv *dv = regs;

    switch (offset) {
    case WIN_DV_CONTROL_OFFSET:
        dv->win_dv_control.reg32 = value;
        break;
    default:
        g_assert_not_reached();
    }
}

static void dv_reset(void *regs)
{
    dv *dv = regs;
    dv->win_dv_control.reg32 = WIN_B_DV_CONTROL_RESET;
}

regs_io_handler dv_handler = {
    .read  = dv_read,
    .write = dv_write,
    .reset = dv_reset,
    .begin = WIN_DV_CONTROL_OFFSET,
    .end   = WIN_DV_CONTROL_OFFSET,
};
