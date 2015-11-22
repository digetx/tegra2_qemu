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

#include "hw/arm/arm.h"
#include "hw/cpu/a9mpcore.h"
#include "hw/intc/gic_internal.h"
#include "hw/ptimer.h"
#include "hw/sysbus.h"
#include "exec/address-spaces.h"
#include "exec/helper-proto.h"
#include "exec/cpu_ldst.h"
#include "sysemu/sysemu.h"

#include "devices.h"
#include "flow.h"
#include "ictlr.h"
#include "iomap.h"
#include "irqs.h"
#include "tegra_cpu.h"
#include "tegra_trace.h"

#define TYPE_TEGRA_FLOW_CTRL "tegra.flow"
#define TEGRA_FLOW_CTRL(obj) OBJECT_CHECK(tegra_flow, (obj), TYPE_TEGRA_FLOW_CTRL)
#define DEFINE_REG32(reg) reg##_t reg
#define WR_MASKED(r, d, m)  r = (r & ~m##_WRMASK) | (d & m##_WRMASK)

/* No flow control */
#define FLOW_MODE_NONE                      0
/* Keep running but generate interrupt when event conditions met */
#define FLOW_MODE_RUN_AND_INT               1
/* Stop running until event conditions met */
#define FLOW_MODE_WAITEVENT                 2
/* Same as FLOW_MODE_WAITEVENT but generate an interrupt when eventd */
#define FLOW_MODE_WAITEVENT_AND_INT         3
/* Stop until an interrupt controller interrupt occurs */
#define FLOW_MODE_STOP_UNTIL_IRQ            4
/* Same as FLOW_MODE_STOP_UNTIL_IRQ but generate another interrupt when eventd */
#define FLOW_MODE_STOP_UNTIL_IRQ_AND_INT    5
/* Stop until event conditions met AND an interrupt controller interrupt occurs */
#define FLOW_MODE_STOP_UNTIL_EVENT_AND_IRQ  6

#define FLOW_EVENT   0x1
#define FLOW_IRQ     0x2

typedef struct tegra_flow_state {
    SysBusDevice parent_obj;

    qemu_irq irq_cpu_event;
    qemu_irq irq_cop_event;
    Notifier wfe_notifier;
    uint8_t sts;

    MemoryRegion iomem;
    ptimer_state *ptimer[TEGRA2_NCPUS];
    DEFINE_REG32(halt_events)[TEGRA2_NCPUS];
    DEFINE_REG32(csr)[TEGRA2_NCPUS];
    DEFINE_REG32(xrq_events);
} tegra_flow;

static const VMStateDescription vmstate_tegra_flow = {
    .name = "tegra.flow",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(sts, tegra_flow),
        VMSTATE_PTIMER_ARRAY(ptimer, tegra_flow, TEGRA2_NCPUS),
        VMSTATE_ARRAY(halt_events, tegra_flow, TEGRA2_NCPUS, 0,
                      vmstate_info_uint32, halt_events_t),
        VMSTATE_ARRAY(csr, tegra_flow, TEGRA2_NCPUS, 0,
                      vmstate_info_uint32, csr_t),
        VMSTATE_UINT32(xrq_events.reg32, tegra_flow),
        VMSTATE_END_OF_LIST()
    }
};

typedef struct tegra_flow_timer_arg {
    tegra_flow *s;
    int cpu_id;
} tegra_flow_timer_arg;

static uint64_t tegra_flow_priv_read(void *opaque, hwaddr offset,
                                     unsigned size)
{
    tegra_flow *s = opaque;
    uint64_t ret = 0;

    assert(size == 4);

    switch (offset) {
    case HALT_CPU0_EVENTS_OFFSET:
        ret = s->halt_events[TEGRA2_A9_CORE0].reg32;
        break;
    case HALT_CPU1_EVENTS_OFFSET:
        ret = s->halt_events[TEGRA2_A9_CORE1].reg32;
        break;
    case HALT_COP_EVENTS_OFFSET:
        ret = s->halt_events[TEGRA2_COP].reg32;
        break;

    case CPU0_CSR_OFFSET:
        ret = s->csr[TEGRA2_A9_CORE0].reg32;
        break;
    case CPU1_CSR_OFFSET:
        ret = s->csr[TEGRA2_A9_CORE1].reg32;
        break;
    case COP_CSR_OFFSET:
        ret = s->csr[TEGRA2_COP].reg32;
        break;

    case XRQ_EVENTS_OFFSET:
        ret = s->xrq_events.reg32;
        break;
    default:
        break;
    }

    TRACE_READ(s->iomem.addr, offset, ret);

    return ret;
}

static __attribute__ ((__unused__)) int tegra_flow_have_pending_irq(void)
{
    A9MPPrivState *a9mpcore = A9MPCORE_PRIV(tegra_a9mpcore_dev);
    GICState *s = &a9mpcore->gic;
    int i;

    for (i = INT_CPU_IRQS_NR; i < INT_GIC_NR; i++) {
        if (!GIC_TEST_ENABLED(i, ALL_CPU_MASK))
            continue;

        if (gic_test_pending(s, i, ALL_CPU_MASK)) {
            TPRINT("tegra_flow: irq %d pending\n", i);
            return 1;
        }
    }

    return 0;
}

static void tegra_flow_gen_interrupt(tegra_flow *s, int cpu_id)
{
    if (cpu_id == TEGRA2_COP && s->halt_events[cpu_id].irq_1) {
        TRACE_IRQ_RAISE(s->iomem.addr, s->irq_cop_event);
        s->csr[cpu_id].intr_flag = 1;
    } else if (s->halt_events[cpu_id].irq_0) {
        TRACE_IRQ_RAISE(s->iomem.addr, s->irq_cpu_event);
        s->csr[cpu_id].intr_flag = 1;
    }
}

void tegra_flow_on_irq(int cpu_id)
{
    tegra_flow *s = tegra_flow_dev;

    if (s->halt_events[cpu_id].mode < FLOW_MODE_STOP_UNTIL_IRQ) {
        return;
    }

//     TPRINT("%s cpu %d\n", __func__, cpu_id);

    switch (s->halt_events[cpu_id].mode) {
    case FLOW_MODE_STOP_UNTIL_IRQ_AND_INT:
        tegra_flow_gen_interrupt(s, cpu_id);
    case FLOW_MODE_STOP_UNTIL_IRQ:
        if (tegra_cpu_is_powergated(cpu_id)) {
            tegra_cpu_unpowergate(cpu_id);
        } else {
            tegra_cpu_unhalt(cpu_id);
        }
        break;
    case FLOW_MODE_STOP_UNTIL_EVENT_AND_IRQ:
        s->sts |= FLOW_IRQ;

        if (s->sts & FLOW_EVENT) {
            if (tegra_cpu_is_powergated(cpu_id)) {
                tegra_cpu_unpowergate(cpu_id);
            } else {
                tegra_cpu_unhalt(cpu_id);
            }
        }
        break;
    default:
        g_assert_not_reached();
        break;
    }
}

static __attribute__ ((__unused__)) const char* tegra_flow_mode_name(int mode)
{
    switch (mode) {
    case FLOW_MODE_NONE:
        return "FLOW_MODE_NONE";
    case FLOW_MODE_RUN_AND_INT:
        return "FLOW_MODE_RUN_AND_INT";
    case FLOW_MODE_WAITEVENT:
        return "FLOW_MODE_WAITEVENT";
    case FLOW_MODE_WAITEVENT_AND_INT:
        return "FLOW_MODE_WAITEVENT_AND_INT";
    case FLOW_MODE_STOP_UNTIL_IRQ:
        return "FLOW_MODE_STOP_UNTIL_IRQ";
    case FLOW_MODE_STOP_UNTIL_IRQ_AND_INT:
        return "FLOW_MODE_STOP_UNTIL_IRQ_AND_INT";
    case FLOW_MODE_STOP_UNTIL_EVENT_AND_IRQ:
        return "FLOW_MODE_STOP_UNTIL_EVENT_AND_IRQ";
    default:
        TPRINT("wrong flow mode\n");
        g_assert_not_reached();
    }

    return "";
}

static void tegra_flow_timer_event(void *opaque)
{
    tegra_flow_timer_arg *arg = opaque;
    tegra_flow *s = arg->s;
    int cpu_id = arg->cpu_id;

    s->halt_events[cpu_id].zero = MAX(0, s->halt_events[cpu_id].zero - 1);

    TPRINT("tegra_flow: event on cpu %d zero=%u event_flag=%d mode=%s\n",
           cpu_id, s->halt_events[cpu_id].zero, s->csr[cpu_id].event_flag,
           tegra_flow_mode_name(s->halt_events[cpu_id].mode));

    if (s->halt_events[cpu_id].zero)
        return;

    s->csr[cpu_id].event_flag = 1;

    switch (s->halt_events[cpu_id].mode) {
    case FLOW_MODE_RUN_AND_INT:
    case FLOW_MODE_WAITEVENT_AND_INT:
        tegra_flow_gen_interrupt(s, cpu_id);
    case FLOW_MODE_WAITEVENT:
        tegra_cpu_unhalt(cpu_id);
        break;

    case FLOW_MODE_STOP_UNTIL_EVENT_AND_IRQ:
        s->sts |= FLOW_EVENT;

        if (s->sts & FLOW_IRQ) {
            if (tegra_cpu_is_powergated(cpu_id)) {
                tegra_cpu_unpowergate(cpu_id);
            } else {
                tegra_cpu_unhalt(cpu_id);
            }
        }
        break;

    default:
        g_assert_not_reached();
        break;
    }

    ptimer_stop(s->ptimer[cpu_id]);
}

static int tegra_flow_arm_event(tegra_flow *s, int cpu_id)
{
    int ev_cnt = s->halt_events[cpu_id].zero;

    if (ev_cnt == 0)
        return 0;

    if (s->halt_events[cpu_id].usec) {
        TPRINT("tegra_flow: cpu %d armed %d usec delay\n", cpu_id, ev_cnt);
        ptimer_set_limit(s->ptimer[cpu_id], 1, 1);
        ptimer_run(s->ptimer[cpu_id], 0);
        return 1;
    }

    if (s->halt_events[cpu_id].msec) {
        TPRINT("tegra_flow: cpu %d armed %d msec delay\n", cpu_id, ev_cnt);
        ptimer_set_limit(s->ptimer[cpu_id], 1000, 1);
        ptimer_run(s->ptimer[cpu_id], 0);
        return 1;
    }

    if (s->halt_events[cpu_id].sec) {
        TPRINT("tegra_flow: cpu %d armed %d sec delay\n", cpu_id, ev_cnt);
        ptimer_set_limit(s->ptimer[cpu_id], 1000000, 1);
        ptimer_run(s->ptimer[cpu_id], 0);
        return 1;
    }

    TPRINT("tegra_flow: implement me!\n");
    g_assert_not_reached();

    return 0;
}

static void tegra_flow_update_events(tegra_flow *s, int cpu_id, int in_wfe)
{
    int mode = s->halt_events[cpu_id].mode;
    int event, irq = 0;

//     TPRINT("%s mode=%s in_wfe=%d cpu %d\n", __func__,
//            tegra_flow_mode_name(mode), in_wfe, cpu_id);

    switch (mode) {
    case FLOW_MODE_NONE:
        tegra_cpu_unhalt(cpu_id);
        break;

    case FLOW_MODE_RUN_AND_INT:
    case FLOW_MODE_WAITEVENT:
    case FLOW_MODE_WAITEVENT_AND_INT:
        tegra_cpu_halt(cpu_id);
        tegra_flow_arm_event(s, cpu_id);
        break;

    case FLOW_MODE_STOP_UNTIL_IRQ_AND_INT:
        irq = tegra_ictlr_is_irq_pending_on_cpu(cpu_id);
    case FLOW_MODE_STOP_UNTIL_IRQ:
        if (s->csr[cpu_id].wait_wfe_bitmap) {
            break;
        }

        if (!irq) {
            tegra_cpu_halt(cpu_id);
        } else if (mode == FLOW_MODE_STOP_UNTIL_IRQ_AND_INT) {
            tegra_flow_gen_interrupt(s, cpu_id);
        }
        break;

    case FLOW_MODE_STOP_UNTIL_EVENT_AND_IRQ:
        if (s->csr[cpu_id].wait_wfe_bitmap) {
            break;
        }

        s->sts = 0;

        event = !tegra_flow_arm_event(s, cpu_id);
        irq = tegra_ictlr_is_irq_pending_on_cpu(cpu_id);

        if (!event || !irq) {
            tegra_cpu_halt(cpu_id);
        }
        break;

    default:
        g_assert_not_reached();
        break;
    }
}

static void tegra_flow_wfe_notify(Notifier *n, void *data)
{
    tegra_flow *s = container_of(n, tegra_flow, wfe_notifier);
    CPUState *cs = data;
    uint32_t wfe_bitmap;
    int cpu_id = cs->cpu_index;

    if (!s->csr[cpu_id].enable)
        return;

    if (!s->csr[cpu_id].wait_wfe_bitmap)
        return;

    wfe_bitmap = tegra_get_wfe_bitmap() | ( 1 << cs->cpu_index);
    wfe_bitmap &= s->csr[cpu_id].wait_wfe_bitmap;

    if (s->csr[cpu_id].wait_wfe_bitmap == wfe_bitmap) {
        tegra_cpu_powergate(cpu_id);

        if (tegra_ictlr_is_irq_pending_on_cpu(cpu_id))
            tegra_cpu_unpowergate(cpu_id);
    }
}

static void tegra_flow_event_write(tegra_flow *s, hwaddr offset,
                                   uint32_t value, int cpu_id)
{
    TRACE_WRITE(s->iomem.addr, offset, s->halt_events[cpu_id].reg32, value);

    s->halt_events[cpu_id].reg32 = value;
    tegra_flow_update_events(s, cpu_id, 0);
}

static void tegra_flow_csr_write(tegra_flow *s, hwaddr offset,
                                 uint32_t value, int cpu_id)
{
    csr_t old_csr;

    TRACE_WRITE(s->iomem.addr, offset, s->csr[cpu_id].reg32, value);

    if (cpu_id != TEGRA2_COP) {
        WR_MASKED(s->csr[cpu_id].reg32, value, CPU_CSR);
    } else {
        WR_MASKED(s->csr[cpu_id].reg32, value, COP_CSR);
    }

    old_csr = s->csr[cpu_id];
//     s->csr[cpu_id].reg32 &= ~(value & 0x3000);

    if (old_csr.intr_flag && !s->csr[cpu_id].intr_flag) {
        switch (cpu_id) {
        case TEGRA2_A9_CORE0:
        case TEGRA2_A9_CORE1:
            TRACE_IRQ_LOWER(s->iomem.addr, s->irq_cpu_event);
            break;
        case TEGRA2_COP:
            TRACE_IRQ_LOWER(s->iomem.addr, s->irq_cop_event);
            break;
        }
    }
}

static void tegra_flow_priv_write(void *opaque, hwaddr offset,
                                  uint64_t value, unsigned size)
{
    tegra_flow *s = opaque;

    assert(size == 4);

    switch (offset) {
    case HALT_CPU0_EVENTS_OFFSET:
        tegra_flow_event_write(s, offset, value, TEGRA2_A9_CORE0);
        break;
    case HALT_CPU1_EVENTS_OFFSET:
        tegra_flow_event_write(s, offset, value, TEGRA2_A9_CORE1);
        break;
    case HALT_COP_EVENTS_OFFSET:
        tegra_flow_event_write(s, offset, value, TEGRA2_COP);
        break;

    case CPU0_CSR_OFFSET:
        tegra_flow_csr_write(s, offset, value, TEGRA2_A9_CORE0);
        break;
    case CPU1_CSR_OFFSET:
        tegra_flow_csr_write(s, offset, value, TEGRA2_A9_CORE1);
        break;
    case COP_CSR_OFFSET:
        tegra_flow_csr_write(s, offset, value, TEGRA2_COP);
        break;

    case XRQ_EVENTS_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->xrq_events.reg32, value);
        s->xrq_events.reg32 = value;
        break;

    default:
        TRACE_WRITE(s->iomem.addr, offset, 0, value);
        break;
    }
}

static void tegra_flow_priv_reset(DeviceState *dev)
{
    tegra_flow *s = TEGRA_FLOW_CTRL(dev);

    s->halt_events[TEGRA2_A9_CORE0].reg32 = HALT_CPU_EVENTS_RESET;
    s->halt_events[TEGRA2_A9_CORE1].reg32 = HALT_CPU_EVENTS_RESET;
    s->csr[TEGRA2_A9_CORE0].reg32 = CPU_CSR_RESET;
    s->csr[TEGRA2_A9_CORE1].reg32 = CPU_CSR_RESET;

    s->halt_events[TEGRA2_COP].reg32 = HALT_COP_EVENTS_RESET;
    s->csr[TEGRA2_COP].reg32 = COP_CSR_RESET;

    s->xrq_events.reg32 = XRQ_EVENTS_RESET;
}

static const MemoryRegionOps tegra_flow_mem_ops = {
    .read = tegra_flow_priv_read,
    .write = tegra_flow_priv_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static int tegra_flow_priv_init(SysBusDevice *dev)
{
    tegra_flow *s = TEGRA_FLOW_CTRL(dev);
    int i;

    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq_cpu_event);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq_cop_event);

    memory_region_init_io(&s->iomem, OBJECT(dev), &tegra_flow_mem_ops, s,
                          "tegra.flow", TEGRA_FLOW_CTRL_SIZE);
    sysbus_init_mmio(dev, &s->iomem);

    for (i = 0; i < TEGRA2_NCPUS; i++) {
        tegra_flow_timer_arg *arg = g_malloc0(sizeof(tegra_flow_timer_arg));
        QEMUBH *bh = qemu_bh_new(tegra_flow_timer_event, arg);

        arg->s = s;
        arg->cpu_id = i;

        s->ptimer[i] = ptimer_init(bh);
        ptimer_set_freq(s->ptimer[i], 1000000);
    }

    s->wfe_notifier.notify = tegra_flow_wfe_notify;
    tegra_register_wfe_notifier(&s->wfe_notifier);

    return 0;
}

static void tegra_flow_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->init = tegra_flow_priv_init;
    dc->vmsd = &vmstate_tegra_flow;
    dc->reset = tegra_flow_priv_reset;
}

static const TypeInfo tegra_flow_info = {
    .name = TYPE_TEGRA_FLOW_CTRL,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(tegra_flow),
    .class_init = tegra_flow_class_init,
};

static void tegra_flow_register_types(void)
{
    type_register_static(&tegra_flow_info);
}

type_init(tegra_flow_register_types)
