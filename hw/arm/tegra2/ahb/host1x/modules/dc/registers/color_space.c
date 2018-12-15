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

#include "color_space.h"

static uint32_t csc_read(void *regs, uint32_t offset)
{
    csc *cs = regs;
    uint32_t ret = 0;

    switch (offset) {
    case WINC_CSC_YOF_OFFSET:
        ret = cs->winc_csc_yof.reg32;
        break;
    case WINC_CSC_KYRGB_OFFSET:
        ret = cs->winc_csc_kyrgb.reg32;
        break;
    case WINC_CSC_KUR_OFFSET:
        ret = cs->winc_csc_kur.reg32;
        break;
    case WINC_CSC_KVR_OFFSET:
        ret = cs->winc_csc_kvr.reg32;
        break;
    case WINC_CSC_KUG_OFFSET:
        ret = cs->winc_csc_kug.reg32;
        break;
    case WINC_CSC_KVG_OFFSET:
        ret = cs->winc_csc_kvg.reg32;
        break;
    case WINC_CSC_KUB_OFFSET:
        ret = cs->winc_csc_kub.reg32;
        break;
    case WINC_CSC_KVB_OFFSET:
        ret = cs->winc_csc_kvb.reg32;
        break;
    default:
        g_assert_not_reached();
    }

    return ret;
}

static void csc_write(void *regs, uint32_t offset, uint32_t value)
{
    csc *cs = regs;

    switch (offset) {
    case WINC_CSC_YOF_OFFSET:
        cs->winc_csc_yof.reg32 = value;
        break;
    case WINC_CSC_KYRGB_OFFSET:
        cs->winc_csc_kyrgb.reg32 = value;
        break;
    case WINC_CSC_KUR_OFFSET:
        cs->winc_csc_kur.reg32 = value;
        break;
    case WINC_CSC_KVR_OFFSET:
        cs->winc_csc_kvr.reg32 = value;
        break;
    case WINC_CSC_KUG_OFFSET:
        cs->winc_csc_kug.reg32 = value;
        break;
    case WINC_CSC_KVG_OFFSET:
        cs->winc_csc_kvg.reg32 = value;
        break;
    case WINC_CSC_KUB_OFFSET:
        cs->winc_csc_kub.reg32 = value;
        break;
    case WINC_CSC_KVB_OFFSET:
        cs->winc_csc_kvb.reg32 = value;
        break;
    default:
        g_assert_not_reached();
    }
}

static void csc_reset(void *regs)
{
    csc *cs = regs;

    cs->winc_csc_yof.reg32 = WINC_CSC_YOF_RESET;
    cs->winc_csc_kyrgb.reg32 = WINC_CSC_KYRGB_RESET;
    cs->winc_csc_kur.reg32 = WINC_CSC_KUR_RESET;
    cs->winc_csc_kvr.reg32 = WINC_CSC_KVR_RESET;
    cs->winc_csc_kug.reg32 = WINC_CSC_KUG_RESET;
    cs->winc_csc_kvg.reg32 = WINC_CSC_KVG_RESET;
    cs->winc_csc_kub.reg32 = WINC_CSC_KUB_RESET;
    cs->winc_csc_kvb.reg32 = WINC_CSC_KVB_RESET;
}

regs_io_handler csc_handler = {
    .read  = csc_read,
    .write = csc_write,
    .reset = csc_reset,
    .begin = 0x611,
    .end   = 0x618,
};
