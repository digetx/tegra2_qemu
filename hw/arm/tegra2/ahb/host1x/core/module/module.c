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

#include <assert.h>

#include "host1x_module.h"

static QLIST_HEAD(, host1x_module) host1x_modules =
    QLIST_HEAD_INITIALIZER(host1x_modules);

void register_host1x_bus_module(struct host1x_module* module, void *opaque)
{
    struct host1x_module* module__;

    module->owner = -1;
    module->opaque = opaque;

    QLIST_FOREACH(module__, &host1x_modules, next) {
        g_assert(module__->class_id != module->class_id);
    }

    QLIST_INSERT_HEAD(&host1x_modules, module, next);
}

struct host1x_module* get_host1x_module(uint32_t class_id)
{
    struct host1x_module* module;

    QLIST_FOREACH(module, &host1x_modules, next) {
        if (module->class_id == class_id)
            return module;
    }

    return NULL;
}

uint32_t host1x_module_read(struct host1x_module* module, uint32_t offset)
{
    assert(module != NULL);
    return module->reg_read(module, offset);
}

void host1x_module_write(struct host1x_module* module,
                         uint32_t offset, uint32_t value)
{
    if (module != NULL) {
        module->reg_write(module, offset, value);
    } else {
        fprintf(stderr, "QEMU: HOST1X: CDMA: %s: module is NULL!!\n", __func__);
    }
}
