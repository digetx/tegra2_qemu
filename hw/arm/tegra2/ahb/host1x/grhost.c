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

#include "hw/sysbus.h"

#include "devices.h"
#include "iomap.h"

#include "host1x_channel.h"
#include "host1x_hwlock.h"
#include "host1x_module.h"
#include "host1x_priv.h"

#include "tegra_trace.h"

#define TYPE_TEGRA_GRHOST "tegra.grhost"
#define TEGRA_GRHOST(obj) OBJECT_CHECK(tegra_grhost, (obj), TYPE_TEGRA_GRHOST)

typedef struct tegra_grhost_state {
    SysBusDevice parent_obj;

    tegra_host1x_channel channels[CHANNELS_NB];

    MemoryRegion container;
    qemu_irq cop_syncpts_irq;
    qemu_irq cpu_syncpts_irq;
    qemu_irq cop_general_irq;
    qemu_irq cpu_general_irq;
} tegra_grhost;

static void tegra_grhost_priv_initfn(Object *obj)
{
    tegra_grhost *s = TEGRA_GRHOST(obj);
    int i;

    memory_region_init(&s->container, obj, "grhost-container",
                       TEGRA_GRHOST_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->container);

    for (i = 0; i < CHANNELS_NB; i++) {
        object_initialize(&s->channels[i], sizeof(s->channels[i]),
                          "tegra.host1x_channel");
        qdev_set_parent_bus(DEVICE(&s->channels[i]), sysbus_get_default());

        host1x_init_cdma(&s->channels[i].cdma, i);
    }
}

static void tegra_grhost_realize_subdev(MemoryRegion *container, void *state,
                                        hwaddr offset)
{
    SysBusDevice *busdev = SYS_BUS_DEVICE(state);

    object_property_set_bool(OBJECT(state), true, "realized", &error_abort);

    memory_region_add_subregion(container, offset,
                                sysbus_mmio_get_region(busdev, 0));
}

static void tegra_grhost_priv_realize(DeviceState *dev, Error **errp)
{
    tegra_grhost *s = TEGRA_GRHOST(dev);
    int i;

    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->cop_syncpts_irq);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->cpu_syncpts_irq);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->cop_general_irq);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->cpu_general_irq);

    host1x_init_syncpts_irq(&s->cpu_syncpts_irq, &s->cop_syncpts_irq);
    host1x_init_mlocks();
    host1x_init_dma();

    for (i = 0; i < CHANNELS_NB; i++)
        tegra_grhost_realize_subdev(&s->container, &s->channels[i], i * SZ_16K);
}

static void tegra_grhost_priv_reset(DeviceState *dev)
{
    host1x_reset_mlocks();
    host1x_reset_hwlocks();
    host1x_reset_syncpt_irqs();
    host1x_reset_modules_irqs();
}

static void tegra_grhost_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = tegra_grhost_priv_realize;
    dc->reset = tegra_grhost_priv_reset;
}

static const TypeInfo tegra_grhost_info = {
    .name = TYPE_TEGRA_GRHOST,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(tegra_grhost),
    .instance_init = tegra_grhost_priv_initfn,
    .class_init = tegra_grhost_class_init,
};

static void tegra_grhost_register_types(void)
{
    type_register_static(&tegra_grhost_info);
}

type_init(tegra_grhost_register_types)
