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
#include "tegra_trace.h"

#include "modules/gr2d/gr2d.h"
#include "modules/host1x/host1x.h"
#include "host1x_channel.h"
#include "host1x_module.h"
#include "host1x_syncpts.h"
#include "host1x_priv.h"

#define TYPE_TEGRA_HOST1X "tegra.host1x"
#define TEGRA_HOST1X(obj) OBJECT_CHECK(tegra_host1x, (obj), TYPE_TEGRA_HOST1X)

typedef struct tegra_host1x_state {
    SysBusDevice parent_obj;

    tegra_host1x_channel channels[CHANNELS_NB];
    tegra_gr2d gr2d;

    MemoryRegion container;
    qemu_irq syncpt_irq;
    qemu_irq general_irq;

    struct host1x_module host1x_module;
} tegra_host1x;

static void tegra_host1x_priv_initfn(Object *obj)
{
    tegra_host1x *s = TEGRA_HOST1X(obj);
    int i;

    memory_region_init(&s->container, obj, "host1x-container",
                       TEGRA_HOST1X_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->container);

    s->host1x_module.class_id = 0x1,
    s->host1x_module.reg_write = host1x_write;
    s->host1x_module.reg_read = host1x_read;

    register_host1x_bus_module(&s->host1x_module, NULL);

    for (i = 0; i < CHANNELS_NB; i++) {
        object_initialize(&s->channels[i], sizeof(s->channels[i]),
                          "tegra.host1x_channel");
        qdev_set_parent_bus(DEVICE(&s->channels[i]), sysbus_get_default());

        host1x_init_cdma(&s->channels[i].cdma, i);
    }

    object_initialize(&s->gr2d, sizeof(s->gr2d), "tegra.gr2d");
    qdev_set_parent_bus(DEVICE(&s->gr2d), sysbus_get_default());
}

static void tegra_host1x_realize_subdev(MemoryRegion *container, void *state,
                                        hwaddr offset)
{
    SysBusDevice *busdev = SYS_BUS_DEVICE(state);

    object_property_set_bool(OBJECT(state), true, "realized", &error_abort);

    memory_region_add_subregion(container, offset,
                                sysbus_mmio_get_region(busdev, 0));
}

static void tegra_host1x_priv_realize(DeviceState *dev, Error **errp)
{
    tegra_host1x *s = TEGRA_HOST1X(dev);
    int i;

    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->syncpt_irq);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->general_irq);

    host1x_init_syncpts_irq(&s->syncpt_irq);
    host1x_init_mlocks();
    host1x_init_dma();

    for (i = 0; i < CHANNELS_NB; i++)
        tegra_host1x_realize_subdev(&s->container, &s->channels[i], i * SZ_16K);

    tegra_host1x_realize_subdev(&s->container, &s->gr2d, 0x40000);
}

static void tegra_host1x_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = tegra_host1x_priv_realize;
}

static const TypeInfo tegra_host1x_info = {
    .name = TYPE_TEGRA_HOST1X,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(tegra_host1x),
    .instance_init = tegra_host1x_priv_initfn,
    .class_init = tegra_host1x_class_init,
};

static void tegra_host1x_register_types(void)
{
    type_register_static(&tegra_host1x_info);
}

type_init(tegra_host1x_register_types)
