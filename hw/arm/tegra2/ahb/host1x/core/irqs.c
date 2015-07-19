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

#include <glib.h>

#include "hw/irq.h"

#include "host1x_syncpts.h"

#include "tegra_trace.h"

#define MODULES_MASK    0xFFF

#define CPU_IRQ(v)  ((1 << 0) | ((!!v) << 1))
#define COP_IRQ(v)  ((1 << 2) | ((!!v) << 3))

static uint32_t host1x_sync_irq_status;
static uint32_t host1x_modules_irq_mask;
static uint32_t host1x_modules_percpu_irq_mask[2];

static uint32_t host1x_syncpts_irq_status[2];
static uint64_t host1x_syncpts_dst_mask;

static qemu_irq *syncpts_irq;

static QemuMutex irq_mutex;

static void host1x_set_irq_status_bit(int cpu_id, bool enable)
{
    uint32_t irq_mask = 1 << (30 + cpu_id);
    int sts_updated;

    g_assert(irq_mask & 0xC0001FFF);

    if (enable) {
        sts_updated = !(host1x_sync_irq_status & irq_mask);
        host1x_sync_irq_status |= irq_mask;
    } else {
        sts_updated = !!(host1x_sync_irq_status & irq_mask);
        host1x_sync_irq_status &= ~irq_mask;
    }

    if (!sts_updated)
        return;

    switch (cpu_id) {
        case HOST1X_CPU:
            TRACE_IRQ_SET(0x50003000, *syncpts_irq, CPU_IRQ(enable));
            break;
        case HOST1X_COP:
            TRACE_IRQ_SET(0x50003000, *syncpts_irq, COP_IRQ(enable));
            break;
        default:
            g_assert_not_reached();
    }
}

inline uint32_t host1x_get_sync_irq_status(void)
{
    return host1x_sync_irq_status;
}

inline uint32_t host1x_get_modules_irq_mask(void)
{
    return host1x_modules_irq_mask;
}

inline uint32_t host1x_get_modules_irq_cpu_mask(void)
{
    return host1x_modules_percpu_irq_mask[HOST1X_CPU];
}

inline uint32_t host1x_get_modules_irq_cop_mask(void)
{
    return host1x_modules_percpu_irq_mask[HOST1X_COP];
}

inline uint32_t host1x_get_syncpts_cpu_irq_status(void)
{
    return host1x_syncpts_irq_status[HOST1X_CPU];
}

inline uint32_t host1x_get_syncpts_cop_irq_status(void)
{
    return host1x_syncpts_irq_status[HOST1X_COP];
}

inline uint32_t host1x_get_syncpts_dst_mask_low(void)
{
    return host1x_syncpts_dst_mask & 0xFFFFFFFF;
}

inline uint32_t host1x_get_syncpts_dst_mask_high(void)
{
    return (host1x_syncpts_dst_mask >> 32) & 0xFFFFFFFF;
}

void host1x_set_modules_irq_mask(uint32_t mask)
{
    qemu_mutex_lock(&irq_mutex);

    host1x_modules_irq_mask = mask;

    qemu_mutex_unlock(&irq_mutex);
}

void host1x_set_modules_percpu_irq_mask(int cpu_id, uint32_t mask)
{
    qemu_mutex_lock(&irq_mutex);

    host1x_modules_percpu_irq_mask[cpu_id] = mask & MODULES_MASK;

    qemu_mutex_unlock(&irq_mutex);
}

void host1x_enable_syncpts_irq_mask(int cpu_id, uint32_t enable_mask)
{
    unsigned i;

    TPRINT("%s: 0x%08X\n", __func__, enable_mask);

    qemu_mutex_lock(&irq_mutex);

    FOREACH_BIT_SET(enable_mask, i, NV_HOST1X_SYNCPT_NB_PTS)
        host1x_syncpts_dst_mask |= 1 << (i * 2 + cpu_id);

    qemu_mutex_unlock(&irq_mutex);

    FOREACH_BIT_SET(enable_mask, i, NV_HOST1X_SYNCPT_NB_PTS) {
        if (host1x_syncpt_threshold_is_crossed(i))
            host1x_set_syncpt_irq(i);
    }
}

void host1x_set_syncpts_irq_dst_mask(uint32_t mask, uint8_t part)
{
    g_assert(part < 2);

    qemu_mutex_lock(&irq_mutex);

    host1x_syncpts_dst_mask &= ~(0xFFFFFFFF << part);
    host1x_syncpts_dst_mask |= mask << part;

    qemu_mutex_unlock(&irq_mutex);
}

void host1x_clear_syncpts_irq_dst_mask(uint32_t clear_mask)
{
    uint32_t i;

    qemu_mutex_lock(&irq_mutex);

    FOREACH_BIT_SET(clear_mask, i, NV_HOST1X_SYNCPT_NB_PTS)
        host1x_syncpts_dst_mask &= ~(3 << (i * 2));

    qemu_mutex_unlock(&irq_mutex);
}

void host1x_set_syncpt_irq(uint8_t syncpt_id)
{
    uint32_t syncpt_id_irq_mask = 1 << syncpt_id;

    TPRINT("%s: %d\n", __func__, syncpt_id);

    qemu_mutex_lock(&irq_mutex);

    if (host1x_syncpts_dst_mask & (1 << (syncpt_id * 2 + HOST1X_CPU))) {
        host1x_syncpts_irq_status[HOST1X_CPU] |= syncpt_id_irq_mask;
        host1x_set_irq_status_bit(HOST1X_CPU, 1);
    }

    if (host1x_syncpts_dst_mask & (1 << (syncpt_id * 2 + HOST1X_COP))) {
        host1x_syncpts_irq_status[HOST1X_COP] |= syncpt_id_irq_mask;
        host1x_set_irq_status_bit(HOST1X_COP, 1);
    }

    qemu_mutex_unlock(&irq_mutex);
}

static void host1x_update_irq(int cpu_id)
{
    unsigned i;

    FOREACH_BIT_SET(host1x_syncpts_irq_status[cpu_id], i, NV_HOST1X_SYNCPT_NB_PTS) {
        if (host1x_syncpts_dst_mask & (1 << (i * 2 + cpu_id))) {
            host1x_set_irq_status_bit(cpu_id, 1);
            return;
        }
    }

    host1x_set_irq_status_bit(cpu_id, 0);
}

void host1x_clear_syncpts_irq_status(int cpu_id, uint32_t clear_mask)
{
    int i;

    qemu_mutex_lock(&irq_mutex);

    FOREACH_BIT_SET(clear_mask, i, NV_HOST1X_SYNCPT_NB_PTS) {
        if (!(host1x_syncpts_dst_mask & (1 << (i * 2 + cpu_id))))
            continue;

        /* Don't clear if IRQ line is enabled and active.  */
        if (host1x_syncpt_threshold_is_crossed(i))
            clear_mask &= ~(1 << i);
    }

    host1x_syncpts_irq_status[cpu_id] &= ~clear_mask;

    host1x_update_irq(cpu_id);

    qemu_mutex_unlock(&irq_mutex);
}

void host1x_init_syncpts_irq(qemu_irq *irq)
{
    qemu_mutex_init(&irq_mutex);

    syncpts_irq = irq;
}

void host1x_reset_irqs(void)
{
    host1x_sync_irq_status = 0;
    host1x_modules_irq_mask = 0;
    host1x_modules_percpu_irq_mask[HOST1X_CPU] = 0;
    host1x_modules_percpu_irq_mask[HOST1X_COP] = 0;
    host1x_syncpts_dst_mask = 0;
}
