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

#include "iomap.h"

#include "host1x_module.h"

#include "host1x.h"

#include "tegra_trace.h"

#define TYPE_TEGRA_HOST1X "tegra.host1x"
#define TEGRA_HOST1X(obj) OBJECT_CHECK(tegra_host1x, (obj), TYPE_TEGRA_HOST1X)

typedef struct tegra_host1x_state {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    struct host1x_regs regs;
    struct host1x_module host1x_module;
} tegra_host1x;

static const VMStateDescription vmstate_tegra_host1x = {
    .name = "tegra-host1x",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(regs.indctrl.reg32, tegra_host1x),
        VMSTATE_UINT32(regs.indoffset, tegra_host1x),
        VMSTATE_UINT8(regs.class_id, tegra_host1x),
        VMSTATE_END_OF_LIST()
    }
};

static uint64_t tegra_host1x_priv_read(void *opaque, hwaddr offset, unsigned size)
{
    tegra_host1x *s = opaque;

    return host1x_module_read(&s->host1x_module, offset >> 2);
}

static void tegra_host1x_priv_write(void *opaque, hwaddr offset,
                                  uint64_t value, unsigned size)
{
    tegra_host1x *s = opaque;

    host1x_module_write(&s->host1x_module, offset >> 2, value);
}

static const MemoryRegionOps tegra_host1x_mem_ops = {
    .read = tegra_host1x_priv_read,
    .write = tegra_host1x_priv_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void tegra_host1x_priv_realize(DeviceState *dev, Error **errp)
{
    tegra_host1x *s = TEGRA_HOST1X(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &tegra_host1x_mem_ops, s,
                          "tegra.host1x", TEGRA_HOST1X_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);

    s->host1x_module.class_id = 0x1,
    s->host1x_module.reg_write = host1x_write;
    s->host1x_module.reg_read = host1x_read;

    register_host1x_bus_module(&s->host1x_module, &s->regs);
}

static void tegra_host1x_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = tegra_host1x_priv_realize;
    dc->vmsd = &vmstate_tegra_host1x;
}

static const TypeInfo tegra_host1x_info = {
    .name = TYPE_TEGRA_HOST1X,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(tegra_host1x),
    .class_init = tegra_host1x_class_init,
};

static void tegra_host1x_register_types(void)
{
    type_register_static(&tegra_host1x_info);
}

type_init(tegra_host1x_register_types)
