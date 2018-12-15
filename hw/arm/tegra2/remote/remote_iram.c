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
#include "iomap.h"
#include "remote_io.h"
#include "tegra_trace.h"

#define TYPE_TEGRA_REMOTE_IRAM "tegra.remote_iram"
#define TEGRA_REMOTE_IRAM(obj) OBJECT_CHECK(remote_iram, (obj), TYPE_TEGRA_REMOTE_IRAM)

/* Keep chunk, used by AVP firmware, locally for faster access.
 * WARNING: IRAM is used by VDE (0x4xx, 0x14xx) and maybe some other HW,
 * carefulness required!!!  */
#define LOCAL_START (0x600 - TEGRA_RESET_HANDLER_SIZE)
#define LOCAL_END   (0x3000 - TEGRA_RESET_HANDLER_SIZE)
#define LOCAL_ENABLED

typedef struct remote_iram_state {
    SysBusDevice parent_obj;

    uint8_t local[LOCAL_END - LOCAL_START];
    MemoryRegion iomem;
} remote_iram;

static uint64_t remote_iram_read(void *opaque, hwaddr offset,
                                 unsigned size)
{
    remote_iram *s = TEGRA_REMOTE_IRAM(opaque);
    uint32_t ret;
#ifdef LOCAL_ENABLED
    uint16_t *local16 = (uint16_t *) (s->local + offset - LOCAL_START);
    uint32_t *local32 = (uint32_t *) (s->local + offset - LOCAL_START);

    if (
#if LOCAL_START > 0
        (offset < LOCAL_START && (offset + size > LOCAL_START)) ||
#endif
        (offset < LOCAL_END && (offset + size > LOCAL_END)))
    {
        hw_error("Unaligned read not implemented");
    }

    switch (offset) {
    case LOCAL_START ... LOCAL_END - 1:
        switch (size) {
        case 1:
//             printf("read%d offset 0x%08X value 0x%08X\n",
//                    size, (uint32_t) offset, (uint32_t) s->local[offset]);
            ret = s->local[offset - LOCAL_START];
            break;
        case 2:
//             printf("read%d offset 0x%08X value 0x%08X\n",
//                    size, (uint32_t) offset, (uint32_t) *(local16));
            ret = *(local16);
            break;
        case 4:
//             printf("read%d offset 0x%08X value 0x%08X\n",
//                    size, (uint32_t) offset, (uint32_t) *(local32));
            ret = *(local32);
            break;
        default:
            g_assert_not_reached();
        }
        break;
    default:
        ret = remote_io_read(s->iomem.addr + offset, size << 3);
        break;
    }
#else
    ret = remote_io_read(s->iomem.addr + offset, size << 3);

    TRACE_READ_MEM(s->iomem.addr, offset, ret, size);
#endif

    return ret;
}

static void remote_iram_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    remote_iram *s = TEGRA_REMOTE_IRAM(opaque);
#ifdef LOCAL_ENABLED
    uint16_t *local16 = (uint16_t *) (s->local + offset - LOCAL_START);
    uint32_t *local32 = (uint32_t *) (s->local + offset - LOCAL_START);

    if (
#if LOCAL_START > 0
        (offset < LOCAL_START && (offset + size > LOCAL_START)) ||
#endif
        (offset < LOCAL_END && (offset + size > LOCAL_END)))
    {
        hw_error("Unaligned write not implemented");
    }

//     printf("write%d offset 0x%08X value 0x%08X\n",
//            size, (uint32_t) offset, (uint32_t) value);

    switch (offset) {
    case LOCAL_START ... LOCAL_END - 1:
        switch (size) {
        case 1:
            s->local[offset - LOCAL_START] = value;
            return;
        case 2:
            *(local16) = value;
            return;
        case 4:
            *(local32) = value;
            return;
        default:
            g_assert_not_reached();
        }
    default:
        break;
    }
#endif

    TRACE_WRITE_MEM(s->iomem.addr, offset, value, size);

    remote_io_write(value, s->iomem.addr + offset, size << 3);
}

static const MemoryRegionOps remote_iram_iram_ops = {
    .read = remote_iram_read,
    .write = remote_iram_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void remote_iram_priv_realize(DeviceState *dev, Error **errp)
{
    remote_iram *s = TEGRA_REMOTE_IRAM(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &remote_iram_iram_ops, s,
                          "tegra.remote_iram",
                          TEGRA_IRAM_SIZE - TEGRA_RESET_HANDLER_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void remote_iram_remote_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = remote_iram_priv_realize;
}

static const TypeInfo remote_iram_info = {
    .name = TYPE_TEGRA_REMOTE_IRAM,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(remote_iram),
    .class_init = remote_iram_remote_class_init,
};

static void remote_iram_register_types(void)
{
    type_register_static(&remote_iram_info);
}

type_init(remote_iram_register_types)
