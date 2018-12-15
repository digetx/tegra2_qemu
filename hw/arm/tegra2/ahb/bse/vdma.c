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

#include "qemu/osdep.h"
#include "hw/sysbus.h"

#include "clk_rst.h"
#include "tegra_trace.h"

#include "vde.h"

static uint64_t tegra_vdma_read(void *opaque, hwaddr offset,
                                 unsigned size)
{
    tegra_vdma *s = opaque;
    uint32_t ret = s->regs[offset >> 2];
    int rst_set = tegra_rst_asserted(TEGRA20_CLK_VDE);
    int clk_en = tegra_clk_enabled(TEGRA20_CLK_VDE);

    if (!clk_en) {
        ret = 1;
    }

    TRACE_READ_EXT(s->iomem.addr, offset, ret, !clk_en, rst_set);

    return ret;
}

static void tegra_vdma_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    tegra_vdma *s = opaque;
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

static const MemoryRegionOps tegra_vdma_mem_ops = {
    .read = tegra_vdma_read,
    .write = tegra_vdma_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void tegra_vde_vdma_priv_realize(DeviceState *dev, Error **errp)
{
    tegra_vdma *s = TEGRA_VDE_VDMA(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &tegra_vdma_mem_ops, s,
                          "tegra.vde_vdma", 256);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void tegra_vde_vdma_priv_reset(DeviceState *dev)
{
    tegra_vdma *s = TEGRA_VDE_VDMA(dev);
    int i;

    for (i = 0; i < 64; i++) {
        s->regs[i] = 0;
    }

    s->regs[0] = 0x0000113E;
    s->regs[2] = 0x00000008;
    s->regs[3] = 0x00000009;
    s->regs[4] = 0x34434B11;
    s->regs[6] = 0x00000380;
    s->regs[7] = 0x00003C10;
    s->regs[8] = 0x0164513E;
    s->regs[9] = 0xC50008FA;
    s->regs[10] = 0x20805149;
    s->regs[11] = 0x932449D1;
    s->regs[18] = 0x0002D210;
    s->regs[19] = 0x8A08CA07;
    s->regs[20] = 0x10FF0002;
    s->regs[21] = 0x0000000F;
    s->regs[33] = 0x00000001;
    s->regs[34] = 0x10000000;
    s->regs[35] = 0x00000010;
}

static void tegra_vde_vdma_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = tegra_vde_vdma_priv_realize;
    dc->reset = tegra_vde_vdma_priv_reset;
}

static const TypeInfo tegra_vdma_info = {
    .name = TYPE_TEGRA_VDE_VDMA,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(tegra_vdma),
    .class_init = tegra_vde_vdma_class_init,
};

static void tegra_vdma_register_types(void)
{
    type_register_static(&tegra_vdma_info);
}

type_init(tegra_vdma_register_types)
