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
#include "tegra_trace.h"

#define TYPE_TEGRA_VDE_MCE "tegra.mce"
#define TEGRA_VDE_MCE(obj) OBJECT_CHECK(tegra_mce, (obj), TYPE_TEGRA_VDE_MCE)

typedef struct tegra_mce_state {
    SysBusDevice parent_obj;

    uint32_t regs[64];
    MemoryRegion iomem;
} tegra_mce;

static uint64_t tegra_mce_read(void *opaque, hwaddr offset,
                                 unsigned size)
{
    tegra_mce *s = opaque;
    uint32_t ret = s->regs[offset >> 2];
    int rst_set = tegra_rst_asserted(TEGRA20_CLK_VDE);
    int clk_en = tegra_clk_enabled(TEGRA20_CLK_VDE);

    TRACE_READ_EXT(s->iomem.addr, offset, ret, !clk_en, rst_set);

    return ret;
}

static void tegra_mce_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    tegra_mce *s = opaque;
    int rst_set = tegra_rst_asserted(TEGRA20_CLK_VDE);
    int clk_en = tegra_clk_enabled(TEGRA20_CLK_VDE);

    TRACE_WRITE_EXT(s->iomem.addr, offset, s->regs[offset >> 2], value,
                    !clk_en, rst_set);

    if (tegra_rst_asserted(TEGRA20_CLK_VDE)) {
        return;
    }

    if (!tegra_clk_enabled(TEGRA20_CLK_VDE)) {
        return;
    }

    s->regs[offset >> 2] = value;
}

static const MemoryRegionOps tegra_mce_mem_ops = {
    .read = tegra_mce_read,
    .write = tegra_mce_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static int tegra_vde_mce_priv_init(SysBusDevice *dev)
{
    tegra_mce *s = TEGRA_VDE_MCE(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &tegra_mce_mem_ops, s,
                          "tegra.vde_mce", 256);
    sysbus_init_mmio(dev, &s->iomem);

    return 0;
}

static void tegra_vde_mce_priv_reset(DeviceState *dev)
{
    tegra_mce *s = TEGRA_VDE_MCE(dev);
    int i;

    for (i = 0; i < 64; i++) {
        s->regs[i] = 0;
    }

    s->regs[2] = 0x00000001;
}

static void tegra_vde_mce_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->init = tegra_vde_mce_priv_init;
    dc->reset = tegra_vde_mce_priv_reset;
}

static const TypeInfo tegra_mce_info = {
    .name = TYPE_TEGRA_VDE_MCE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(tegra_mce),
    .class_init = tegra_vde_mce_class_init,
};

static void tegra_mce_register_types(void)
{
    type_register_static(&tegra_mce_info);
}

type_init(tegra_mce_register_types)
