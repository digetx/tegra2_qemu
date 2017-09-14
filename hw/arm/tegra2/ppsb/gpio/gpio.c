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
#include "exec/address-spaces.h"
#include "hw/sysbus.h"

#include "gpio.h"
#include "iomap.h"
#include "tegra_trace.h"

#define TYPE_TEGRA_GPIO "tegra.gpio"
#define TEGRA_GPIO(obj) OBJECT_CHECK(tegra_gpio, (obj), TYPE_TEGRA_GPIO)
#define DEFINE_REG32(reg) reg##_t reg

#define BANKS_NB    7
#define PORTS_NB    4

typedef struct tegra_gpio_port_state {
    DEFINE_REG32(gpio_cnf);
    DEFINE_REG32(gpio_oe);
    DEFINE_REG32(gpio_out);
    DEFINE_REG32(gpio_in);
    DEFINE_REG32(gpio_int_sta);
    DEFINE_REG32(gpio_int_enb);
    DEFINE_REG32(gpio_int_lvl);
    DEFINE_REG32(gpio_int_clr);
    DEFINE_REG32(gpio_msk_cnf);
    DEFINE_REG32(gpio_msk_oe);
    DEFINE_REG32(gpio_msk_out);
    DEFINE_REG32(gpio_msk_int_sta);
    DEFINE_REG32(gpio_msk_int_enb);
    DEFINE_REG32(gpio_msk_int_lvl);
} tegra_gpio_port;

static const VMStateDescription vmstate_tegra_gpio_port = {
    .name = "tegra-gpio-port",
    .version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(gpio_cnf.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_oe.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_out.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_in.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_int_sta.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_int_enb.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_int_lvl.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_int_clr.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_msk_cnf.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_msk_oe.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_msk_out.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_msk_int_sta.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_msk_int_enb.reg32, tegra_gpio_port),
        VMSTATE_UINT32(gpio_msk_int_lvl.reg32, tegra_gpio_port),
        VMSTATE_END_OF_LIST()
    }
};

typedef struct tegra_gpio_state {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    tegra_gpio_port regs[BANKS_NB * PORTS_NB];
    qemu_irq irq[BANKS_NB];
} tegra_gpio;

static const VMStateDescription vmstate_tegra_gpio = {
    .name = "tegra.gpio",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_STRUCT_ARRAY(regs, tegra_gpio, BANKS_NB * PORTS_NB, 0,
                             vmstate_tegra_gpio_port, tegra_gpio_port),
        VMSTATE_END_OF_LIST()
    }
};

static uint64_t tegra_gpio_priv_read(void *opaque, hwaddr offset,
                                     unsigned size)
{
    tegra_gpio *s = opaque;
    tegra_gpio_port *p;
    int bank = (offset >> 7) & 0x7;
    int port = (offset & 0xf) >> 2;
    int port_nb = bank * PORTS_NB + port;
    uint64_t ret = 0;

    p = &s->regs[port_nb];

    switch (offset & 0x870) {
    case GPIO_CNF_OFFSET:
        ret = p->gpio_cnf.reg32;
        break;
    case GPIO_OE_OFFSET:
        ret = p->gpio_oe.reg32;
        break;
    case GPIO_OUT_OFFSET:
        ret = p->gpio_out.reg32;
        break;
    case GPIO_IN_OFFSET:
        ret = p->gpio_in.reg32;

        /* VOL KEYS */
        if (port_nb == 16) {
            ret |= (1 << 4);
            ret |= (1 << 5);
        }
        break;
    case GPIO_INT_STA_OFFSET:
        ret = p->gpio_int_sta.reg32;
        break;
    case GPIO_INT_ENB_OFFSET:
        ret = p->gpio_int_enb.reg32;
        break;
    case GPIO_INT_LVL_OFFSET:
        ret = p->gpio_int_lvl.reg32;
        break;
    case GPIO_INT_CLR_OFFSET:
        ret = p->gpio_int_clr.reg32;
        break;
    case GPIO_MSK_CNF_OFFSET:
        ret = p->gpio_msk_cnf.reg32;
        break;
    case GPIO_MSK_OE_OFFSET:
        ret = p->gpio_msk_oe.reg32;
        break;
    case GPIO_MSK_OUT_OFFSET:
        ret = p->gpio_msk_out.reg32;
        break;
    case GPIO_MSK_INT_STA_OFFSET:
        ret = p->gpio_msk_int_sta.reg32;
        break;
    case GPIO_MSK_INT_ENB_OFFSET:
        ret = p->gpio_msk_int_enb.reg32;
        break;
    case GPIO_MSK_INT_LVL_OFFSET:
        ret = p->gpio_msk_int_lvl.reg32;
        break;
    default:
        break;
    }

    TRACE_READ(s->iomem.addr, offset, ret);

    return ret;
}

static void tegra_gpio_priv_write(void *opaque, hwaddr offset,
                                  uint64_t value, unsigned size)
{
    tegra_gpio *s = opaque;
    tegra_gpio_port *p;
    int bank = (offset >> 7) & 0x7;
    int port = (offset & 0xf) >> 2;

    p = &s->regs[bank * PORTS_NB + port];

    switch (offset & 0x870) {
    case GPIO_CNF_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_cnf.reg32, value);
        p->gpio_cnf.reg32 = value;
        break;
    case GPIO_OE_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_oe.reg32, value);
        p->gpio_oe.reg32 = value;
        break;
    case GPIO_OUT_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_out.reg32, value);
        p->gpio_out.reg32 = value;
        break;
    case GPIO_IN_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_in.reg32, value);
        p->gpio_in.reg32 = value;
        break;
    case GPIO_INT_STA_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_int_sta.reg32, value);
        p->gpio_int_sta.reg32 = value;
        break;
    case GPIO_INT_ENB_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_int_enb.reg32, value);
        p->gpio_int_enb.reg32 = value;
        break;
    case GPIO_INT_LVL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_int_lvl.reg32, value);
        p->gpio_int_lvl.reg32 = value;
        break;
    case GPIO_INT_CLR_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_int_clr.reg32, value);
        p->gpio_int_clr.reg32 = value;
        break;
    case GPIO_MSK_CNF_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_msk_cnf.reg32, value);
        p->gpio_msk_cnf.reg32 = value;
        break;
    case GPIO_MSK_OE_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_msk_oe.reg32, value);
        p->gpio_msk_oe.reg32 = value;
        break;
    case GPIO_MSK_OUT_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_msk_out.reg32, value);
        p->gpio_msk_out.reg32 = value;
        break;
    case GPIO_MSK_INT_STA_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_msk_int_sta.reg32, value);
        p->gpio_msk_int_sta.reg32 = value;
        break;
    case GPIO_MSK_INT_ENB_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_msk_int_enb.reg32, value);
        p->gpio_msk_int_enb.reg32 = value;
        break;
    case GPIO_MSK_INT_LVL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, p->gpio_msk_int_lvl.reg32, value);
        p->gpio_msk_int_lvl.reg32 = value;
        break;
    default:
        TRACE_WRITE(s->iomem.addr, offset, 0, value);
        break;
    }
}

static void tegra_gpio_priv_reset(DeviceState *dev)
{
    tegra_gpio *s = TEGRA_GPIO(dev);
    int i;

    for (i = 0; i < BANKS_NB * PORTS_NB; i++) {
        tegra_gpio_port *p = &s->regs[i];

        p->gpio_cnf.reg32 = GPIO_CNF_RESET;
        p->gpio_oe.reg32 = GPIO_OE_RESET;
        p->gpio_out.reg32 = GPIO_OUT_RESET;
        p->gpio_in.reg32 = GPIO_IN_RESET;
        p->gpio_int_sta.reg32 = GPIO_INT_STA_RESET;
        p->gpio_int_enb.reg32 = GPIO_INT_ENB_RESET;
        p->gpio_int_lvl.reg32 = GPIO_INT_LVL_RESET;
        p->gpio_int_clr.reg32 = GPIO_INT_CLR_RESET;
        p->gpio_msk_cnf.reg32 = GPIO_MSK_CNF_RESET;
        p->gpio_msk_oe.reg32 = GPIO_MSK_OE_RESET;
        p->gpio_msk_out.reg32 = GPIO_MSK_OUT_RESET;
        p->gpio_msk_int_sta.reg32 = GPIO_MSK_INT_STA_RESET;
        p->gpio_msk_int_enb.reg32 = GPIO_MSK_INT_ENB_RESET;
        p->gpio_msk_int_lvl.reg32 = GPIO_MSK_INT_LVL_RESET;
    }
}

static const MemoryRegionOps tegra_gpio_mem_ops = {
    .read = tegra_gpio_priv_read,
    .write = tegra_gpio_priv_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void tegra_gpio_priv_realize(DeviceState *dev, Error **errp)
{
    tegra_gpio *s = TEGRA_GPIO(dev);
    int i;

    memory_region_init_io(&s->iomem, OBJECT(dev), &tegra_gpio_mem_ops, s,
                          "tegra.gpio", TEGRA_GPIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);

    for (i = 0; i < BANKS_NB; i++)
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq[i]);
}

static void tegra_gpio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = tegra_gpio_priv_realize;
    dc->vmsd = &vmstate_tegra_gpio;
    dc->reset = tegra_gpio_priv_reset;
}

static const TypeInfo tegra_gpio_info = {
    .name = TYPE_TEGRA_GPIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(tegra_gpio),
    .class_init = tegra_gpio_class_init,
};

static void tegra_gpio_register_types(void)
{
    type_register_static(&tegra_gpio_info);
}

type_init(tegra_gpio_register_types)
