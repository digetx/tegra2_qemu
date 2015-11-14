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

#include "hw/arm/arm.h"
#include "hw/sysbus.h"

#include "tcg-op.h"

#include "devices.h"
#include "sizes.h"
#include "tegra_cpu.h"
#include "tegra_trace.h"

#define TYPE_TEGRA_CCH "tegra.cch"
#define TEGRA_CCH(obj) OBJECT_CHECK(tegra_cch, (obj), TYPE_TEGRA_CCH)

#define PTE0_COMPARE    0xF000
#define PTE0_TRANSLATE  0xF004

#define TRANSLATE_DATA  (1 << 11)
#define TRANSLATE_CODE  (1 << 10)
#define TRANSLATE_WR    (1 << 9)
#define TRANSLATE_RD    (1 << 8)
#define TRANSLATE_HIT   (1 << 7)
#define TRANSLATE_EN    (1 << 2)

typedef struct tegra_cch_state {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint16_t translate_virt_base;
    uint16_t translate_phys_base;
    uint16_t translate_flags;
    uint16_t translate_mask;
} tegra_cch;

static const VMStateDescription vmstate_tegra_cch = {
    .name = "tegra.cch",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT16(translate_virt_base, tegra_cch),
        VMSTATE_UINT16(translate_phys_base, tegra_cch),
        VMSTATE_UINT16(translate_flags, tegra_cch),
        VMSTATE_UINT16(translate_mask, tegra_cch),
        VMSTATE_END_OF_LIST()
    }
};

static uint64_t tegra_cch_priv_read(void *opaque, hwaddr offset,
                                    unsigned size)
{
    tegra_cch *s = opaque;
    uint64_t ret = 0;

    if (current_cpu != qemu_get_cpu(TEGRA2_COP)) {
        return ret;
    }

    assert(size == 4);

    switch (offset) {
    case PTE0_COMPARE:
        ret = (s->translate_virt_base << 16) | s->translate_mask;
        break;
    case PTE0_TRANSLATE:
        ret = (s->translate_phys_base << 16) | s->translate_flags;
        break;
    default:
        break;
    }

    TRACE_READ(s->iomem.addr, offset, ret);

    return ret;
}

static void tegra_cch_refill_tlb(tegra_cch *s)
{
    const hwaddr phys_base = s->translate_phys_base;
    const hwaddr virt_base = s->translate_virt_base & s->translate_mask;

    tlb_flush(current_cpu, 1);

    if (!(s->translate_flags & TRANSLATE_EN)) {
        goto FIN;
    }

    /* Too much churn to implement correctly.  */
    g_assert(s->translate_flags & TRANSLATE_DATA);
    g_assert(s->translate_flags & TRANSLATE_HIT);
    g_assert(s->translate_flags & TRANSLATE_RD);
    g_assert(s->translate_flags & TRANSLATE_WR);
    g_assert(s->translate_flags & TRANSLATE_CODE);

    /* ??? Locking "no-MMU" mode might be better.  */
    tlb_set_page(current_cpu, virt_base << 16, 0x80000000 | (phys_base << 16),
                 PAGE_BITS, ARMMMUIdx_S12NSE1, SZ_1M);

FIN:
    if (tcg_enabled()) {
        if (tcg_ctx.gen_next_op_idx != OPC_BUF_SIZE) {
            tcg_gen_exit_tb(0);
        }
    }
}

static void tegra_cch_priv_write(void *opaque, hwaddr offset,
                                 uint64_t value, unsigned size)
{
    tegra_cch *s = opaque;
    uint32_t old;

    if (current_cpu != qemu_get_cpu(TEGRA2_COP)) {
        return;
    }

    assert(size == 4);

    switch (offset) {
    case PTE0_COMPARE:
        old = (s->translate_virt_base << 16) | s->translate_mask;

        TRACE_WRITE(s->iomem.addr, offset, old, value);

        s->translate_virt_base = value >> 16;
        s->translate_mask = value & 0x3FFF;
        break;
    case PTE0_TRANSLATE:
        old = (s->translate_phys_base << 16) | s->translate_flags;

        TRACE_WRITE(s->iomem.addr, offset, old, value);

        s->translate_phys_base = (value >> 16) & 0x3FFF;
        s->translate_flags = value & 0xFFF;
        break;
    default:
        TRACE_WRITE(s->iomem.addr, offset, 0, value);
        return;
    }

    tegra_cch_refill_tlb(s);
}

static void tegra_cch_priv_reset(DeviceState *dev)
{
    tegra_cch *s = TEGRA_CCH(dev);

    s->translate_virt_base = 0;
    s->translate_phys_base = 0;
    s->translate_flags = 0;
    s->translate_mask = 0;
}

static const MemoryRegionOps tegra_cch_mem_ops = {
    .read = tegra_cch_priv_read,
    .write = tegra_cch_priv_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static hwaddr tegra_cch_translate(hwaddr addr, int access_type)
{
    tegra_cch *s = tegra_cch_dev;
    const hwaddr phys_base = s->translate_phys_base;
    const hwaddr virt_base = s->translate_virt_base & s->translate_mask;

    if (!(s->translate_flags & TRANSLATE_EN)) {
        return addr;
    }

    if ((addr >> 16) != virt_base) {
        return addr;
    }

    addr &= 0x0000FFFF;
    addr |= 0x80000000 | (phys_base << 16);

    return addr;
}

static int tegra_cch_priv_init(SysBusDevice *dev)
{
    tegra_cch *s = TEGRA_CCH(dev);
    CPUState *cs = qemu_get_cpu(TEGRA2_COP);

    memory_region_init_io(&s->iomem, OBJECT(dev), &tegra_cch_mem_ops, s,
                          "tegra.cch", SZ_64K);
    sysbus_init_mmio(dev, &s->iomem);

    ARM_CPU(cs)->translate_addr = tegra_cch_translate;

    return 0;
}

static void tegra_cch_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->init = tegra_cch_priv_init;
    dc->vmsd = &vmstate_tegra_cch;
    dc->reset = tegra_cch_priv_reset;
}

static const TypeInfo tegra_cch_info = {
    .name = TYPE_TEGRA_CCH,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(tegra_cch),
    .class_init = tegra_cch_class_init,
};

static void tegra_cch_register_types(void)
{
    type_register_static(&tegra_cch_info);
}

type_init(tegra_cch_register_types)
