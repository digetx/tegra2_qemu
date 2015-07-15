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

#include "qemu-common.h"

#include "modules/host1x/host1x.h"

#include "host1x_channel.h"
#include "host1x_fifo.h"
#include "host1x_module.h"
#include "host1x_syncpts.h"

#include "iomap.h"
#include "tegra_trace.h"

#define TYPE_TEGRA_HOST1X_CHANNEL "tegra.host1x_channel"
#define TEGRA_HOST1X_CHANNEL(obj) OBJECT_CHECK(tegra_host1x_channel, (obj), TYPE_TEGRA_HOST1X_CHANNEL)

#define CHANNEL_BASE    (0x50000000 | (s->cdma.ch_id * SZ_16K))

static const VMStateDescription vmstate_tegra_host1x_channel = {
    .name = "tegra.host1x_channel",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(fifostat.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(indoff.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(indcnt.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(dmastart.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(dmaput.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(dmaend.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(dmactrl.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(indoff2.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(tickcount_hi.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(tickcount_lo.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(channelctrl.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(raise.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(fbbufbase.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(cmdswap.reg32, tegra_host1x_channel),
        VMSTATE_UINT32(indoffset, tegra_host1x_channel),
        VMSTATE_UINT8(class_id, tegra_host1x_channel),
        VMSTATE_END_OF_LIST()
    }
};

static uint32_t ind_swap(uint32_t value, int type)
{
    uint32_t ret;

    switch (type) {
        case 1:
            ret = (bswap16(value >> 16) << 16) | bswap16(value & 0xffff);
            break;
        case 2:
            ret = bswap32(value);
            break;
        case 3:
            ret = ror32(value, 16);
            break;
        default:
            ret = value;
            break;
    }

    return ret;
}

static uint64_t tegra_host1x_channel_priv_read(void *opaque, hwaddr offset,
                                               unsigned size)
{
    tegra_host1x_channel *s = opaque;
    uint64_t ret = 0;

    assert(size == 4);

    switch (offset) {
    case FIFOSTAT_OFFSET:
        s->fifostat.outfentries = host1x_get_fifo_entries_nb(s->fifo);
        ret = s->fifostat.reg32;
        break;
    case INDOFF_OFFSET:
        ret = s->indoff.reg32;
        break;
    case INDCNT_OFFSET:
        ret = s->indcnt.reg32;
        break;
    case INDDATA_OFFSET:
        ret = host1x_fifo_pop(s->fifo);
        break;
    case DMASTART_OFFSET:
        ret = s->dmastart.reg32;
        break;
    case DMAPUT_OFFSET:
        ret = s->dmaput.reg32;
        break;
    case DMAGET_OFFSET:
        ret = s->cdma.gather.get << 2;
        break;
    case DMAEND_OFFSET:
        ret = s->dmaend.reg32;
        break;
    case DMACTRL_OFFSET:
        ret = s->dmactrl.reg32;
        break;
    case INDOFF2_OFFSET:
        ret = s->indoff2.reg32;
        break;
    case TICKCOUNT_HI_OFFSET:
        ret = s->tickcount_hi.reg32;
        break;
    case TICKCOUNT_LO_OFFSET:
        ret = s->tickcount_lo.reg32;
        break;
    case CHANNELCTRL_OFFSET:
        ret = s->channelctrl.reg32;
        break;
    case RAISE_OFFSET:
        ret = s->raise.reg32;
        break;
    case FBBUFBASE_OFFSET:
        ret = s->fbbufbase.reg32;
        break;
    case CMDSWAP_OFFSET:
        ret = s->cmdswap.reg32;
        break;
    case 0x3000 ... 0x38E8:
        return host1x_sync_read_reg(CHANNEL_BASE + offset);
    default:
        break;
    }

    TRACE_READ(CHANNEL_BASE, offset, ret);

    return ret;
}

static void tegra_host1x_channel_priv_write(void *opaque, hwaddr offset,
                                            uint64_t value, unsigned size)
{
    tegra_host1x_channel *s = opaque;

    assert(size == 4);

    switch (offset) {
    case INDOFF_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->indoff.reg32, value);
        s->indoff.reg32 = value;

        if (s->indoff.indoffupd == UPDATE) {
            if (s->indoff.acctype == REG) {
                s->indoffset = s->indoff.regoffset;
                s->class_id = decode_class_id(s->indoff.modid);
            } else {
                s->indoffset = s->indoff.fboffset;
            }
        }
        break;
    case INDCNT_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->indcnt.reg32, value);
        s->indcnt.reg32 = value;

        g_assert_not_reached();
        break;
    case INDDATA_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->inddata.reg32, value);

        if (s->indoff.acctype == REG) {
            /* Indirect host1x module reg write */
            struct host1x_module *module = get_host1x_module(s->class_id);

            host1x_module_write(module, s->indoffset, value);
        } else {
            /* Indirect memory write */
            uint32_t *mem = host1x_dma_ptr;

            mem[s->indoffset] = ind_swap(value, s->indoff.indswap);
        }

        if (s->indoff.autoinc) {
            s->indoffset++;
            s->indoffset &= 0x3fffffff;
        }
        break;
    case DMASTART_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->dmastart.reg32, value);
        s->dmastart.reg32 = value;

        host1x_cdma_set_base(&s->cdma, s->dmastart.dmastart);
        break;
    case DMAPUT_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->dmaput.reg32, value);
        s->dmaput.reg32 = value;

        host1x_cdma_set_put(&s->cdma, s->dmaput.dmaput);
        break;
    case DMAEND_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->dmaend.reg32, value);
        s->dmaend.reg32 = value;

        host1x_cdma_set_end(&s->cdma, s->dmaend.dmaend);
        break;
    case DMACTRL_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->dmactrl.reg32, value);
        s->dmactrl.reg32 = value;

        host1x_cdma_control(&s->cdma, s->dmactrl.dmastop, s->dmactrl.dmagetrst,
                            s->dmactrl.dmainitget);
        break;
    case INDOFF2_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->indoff2.reg32, value);
        s->indoff2.reg32 = value;

        if (s->indoff.acctype == REG) {
            s->indoffset = s->indoff2.regoffset;
            s->class_id = decode_class_id(s->indoff2.modid);
        } else {
            s->indoffset = s->indoff2.fboffset;
        }
        break;
    case TICKCOUNT_HI_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->tickcount_hi.reg32, value);
        s->tickcount_hi.reg32 = value;
        break;
    case TICKCOUNT_LO_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->tickcount_lo.reg32, value);
        s->tickcount_lo.reg32 = value;
        break;
    case CHANNELCTRL_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->channelctrl.reg32, value);
        s->channelctrl.reg32 = value;
        break;
    case RAISE_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->raise.reg32, value);
        s->raise.reg32 = value;
        break;
    case FBBUFBASE_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->fbbufbase.reg32, value);
        s->fbbufbase.reg32 = value;
        break;
    case CMDSWAP_OFFSET:
        TRACE_WRITE(CHANNEL_BASE, offset, s->cmdswap.reg32, value);
        s->cmdswap.reg32 = value;
        break;
    case 0x3000 ... 0x38E8:
        host1x_sync_write_reg(CHANNEL_BASE + offset, value);
        break;
    default:
        TRACE_WRITE(CHANNEL_BASE, offset, 0, value);
        break;
    }
}

static void tegra_host1x_channel_priv_reset(DeviceState *dev)
{
    tegra_host1x_channel *s = TEGRA_HOST1X_CHANNEL(dev);

    host1x_cdma_control(&s->cdma, 1, 1, 0);

    s->fifostat.reg32 = FIFOSTAT_RESET;
    s->indoff.reg32 = INDOFF_RESET;
    s->indcnt.reg32 = INDCNT_RESET;
    s->dmastart.reg32 = DMASTART_RESET;
    s->dmaput.reg32 = DMAPUT_RESET;
    s->dmaend.reg32 = DMAEND_RESET;
    s->dmactrl.reg32 = DMACTRL_RESET;
    s->indoff2.reg32 = INDOFF2_RESET;
    s->tickcount_hi.reg32 = TICKCOUNT_HI_RESET;
    s->tickcount_lo.reg32 = TICKCOUNT_LO_RESET;
    s->channelctrl.reg32 = CHANNELCTRL_RESET;
    s->raise.reg32 = RAISE_RESET;
    s->fbbufbase.reg32 = FBBUFBASE_RESET;
    s->cmdswap.reg32 = CMDSWAP_RESET;

    host1x_cdma_set_base(&s->cdma, s->dmastart.dmastart);
    host1x_cdma_set_put(&s->cdma, s->dmaput.dmaput);
    host1x_cdma_set_end(&s->cdma, s->dmaend.dmaend);

    host1x_fifo_reset(s->fifo);
}

static const MemoryRegionOps tegra_host1x_channel_mem_ops = {
    .read = tegra_host1x_channel_priv_read,
    .write = tegra_host1x_channel_priv_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void tegra_host1x_channel_realize(DeviceState *dev, Error **errp)
{
    tegra_host1x_channel *s = TEGRA_HOST1X_CHANNEL(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &tegra_host1x_channel_mem_ops,
                          s, "tegra.host1x_channel", SZ_16K);
    sysbus_init_mmio(sbd, &s->iomem);

    s->fifo = host1x_fifo_create(32);
}

static Property tegra_host1x_channel_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void tegra_host1x_channel_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->props = tegra_host1x_channel_properties;
    dc->vmsd = &vmstate_tegra_host1x_channel;
    dc->realize = tegra_host1x_channel_realize;
    dc->reset = tegra_host1x_channel_priv_reset;
}

static const TypeInfo tegra_host1x_channel_info = {
    .name = TYPE_TEGRA_HOST1X_CHANNEL,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(tegra_host1x_channel),
    .class_init = tegra_host1x_channel_class_init,
};

static void tegra_host1x_channel_register_types(void)
{
    type_register_static(&tegra_host1x_channel_info);
}

type_init(tegra_host1x_channel_register_types)
