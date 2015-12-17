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

#define TYPE_TEGRA_VDE_TFE "tegra.tfe"
#define TEGRA_VDE_TFE(obj) OBJECT_CHECK(tegra_tfe, (obj), TYPE_TEGRA_VDE_TFE)

typedef struct tegra_tfe_state {
    SysBusDevice parent_obj;

    uint32_t regs[64];
    MemoryRegion iomem;
} tegra_tfe;

static uint64_t tegra_tfe_read(void *opaque, hwaddr offset,
                                 unsigned size)
{
    tegra_tfe *s = opaque;
    uint32_t ret = s->regs[offset >> 2];
    int rst_set = tegra_rst_asserted(TEGRA20_CLK_VDE);
    int clk_en = tegra_clk_enabled(TEGRA20_CLK_VDE);

    TRACE_READ_EXT(s->iomem.addr, offset, ret, !clk_en, rst_set);

    return ret;
}

static void tegra_tfe_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    tegra_tfe *s = opaque;
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

static const MemoryRegionOps tegra_tfe_mem_ops = {
    .read = tegra_tfe_read,
    .write = tegra_tfe_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static int tegra_vde_tfe_priv_init(SysBusDevice *dev)
{
    tegra_tfe *s = TEGRA_VDE_TFE(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &tegra_tfe_mem_ops, s,
                          "tegra.vde_tfe", 256);
    sysbus_init_mmio(dev, &s->iomem);

    return 0;
}

static void tegra_vde_tfe_priv_reset(DeviceState *dev)
{
    tegra_tfe *s = TEGRA_VDE_TFE(dev);
    int i;

    for (i = 0; i < 64; i++) {
        s->regs[i] = 0;
    }

    s->regs[0] = 0x00000107;
}

static void tegra_vde_tfe_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->init = tegra_vde_tfe_priv_init;
    dc->reset = tegra_vde_tfe_priv_reset;
}

static const TypeInfo tegra_tfe_info = {
    .name = TYPE_TEGRA_VDE_TFE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(tegra_tfe),
    .class_init = tegra_vde_tfe_class_init,
};

static void tegra_tfe_register_types(void)
{
    type_register_static(&tegra_tfe_info);
}

type_init(tegra_tfe_register_types)
