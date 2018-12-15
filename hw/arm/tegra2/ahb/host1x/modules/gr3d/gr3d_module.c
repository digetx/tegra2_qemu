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

#include "qemu/osdep.h"
#include "hw/sysbus.h"

#include "host1x_module.h"
#include "host1x_syncpts.h"

#include "tegra_trace.h"

static void gr3d_write(struct host1x_module *module,
                         uint32_t offset, uint32_t data)
{
    TRACE_WRITE(module->class_id, offset, data, data);

    switch (offset) {
    case 0:
        host1x_incr_syncpt(data & 0xff);
        break;
    default:
//         g_assert_not_reached();
        break;
    }
}

static uint32_t gr3d_read(struct host1x_module *module, uint32_t offset)
{
    TRACE_READ(module->class_id, offset, 0);
    return 0;
}

static struct host1x_module gr3d_module = {
    .class_id = 0x60,
    .reg_write = gr3d_write,
    .reg_read = gr3d_read,
};

static void register_gr3d_module(void)
{
    register_host1x_bus_module(&gr3d_module, NULL);
}

type_init(register_gr3d_module);
