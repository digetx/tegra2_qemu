/*
 * ARM NVIDIA Tegra2 emulation.
 *
 * Copyright (c) 2015 Dmitry Osipenko <digetx@gmail.com>
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

#include "clk_rst.h"
#include "remote_io.h"
#include "tegra_trace.h"

#define TYPE_TEGRA_REMOTE_MEM "tegra.remote_mem"
#define TEGRA_REMOTE_MEM(obj) OBJECT_CHECK(remote_mem, (obj), TYPE_TEGRA_REMOTE_MEM)

typedef struct remote_mem_state {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
} remote_mem;

static uint64_t remote_mem_read(void *opaque, hwaddr offset,
                                unsigned size)
{
    remote_mem *s = TEGRA_REMOTE_MEM(opaque);
    uint32_t ret = remote_io_read(s->iomem.addr + offset, size << 3);

    TRACE_READ_MEM(s->iomem.addr, offset, ret, size);

    return ret;
}

static void remote_mem_write(void *opaque, hwaddr offset,
                             uint64_t value, unsigned size)
{
    remote_mem *s = TEGRA_REMOTE_MEM(opaque);

    TRACE_WRITE_MEM(s->iomem.addr, offset, value, size);

    remote_io_write(value, s->iomem.addr + offset, size << 3);
}

static const MemoryRegionOps remote_mem_mem_ops = {
    .read = remote_mem_read,
    .write = remote_mem_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static int remote_mem_priv_init(SysBusDevice *dev)
{
    remote_mem *s = TEGRA_REMOTE_MEM(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &remote_mem_mem_ops, s,
                          "tegra.remote_mem", 0x10000000);
    sysbus_init_mmio(dev, &s->iomem);

    return 0;
}

static void remote_mem_remote_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = remote_mem_priv_init;
}

static const TypeInfo remote_mem_info = {
    .name = TYPE_TEGRA_REMOTE_MEM,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(remote_mem),
    .class_init = remote_mem_remote_class_init,
};

static void remote_mem_register_types(void)
{
    type_register_static(&remote_mem_info);
}

type_init(remote_mem_register_types)
