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

#define CPU 0
#define COP 1

#define CPU_IRQ(v)  ((1 << 0) | ((!!v) << 1))
#define COP_IRQ(v)  ((1 << 2) | ((!!v) << 3))

static uint32_t host1x_sync_irq_status;
static uint32_t host1x_modules_irq_mask;
static uint32_t host1x_modules_irq_cpu_mask;
static uint32_t host1x_modules_irq_cop_mask;

static uint32_t host1x_syncpts_cpu_irq_status;
static uint32_t host1x_syncpts_cop_irq_status;
static uint64_t host1x_syncpts_dst_mask;

static qemu_irq *syncpts_irq;

static QemuMutex irq_mutex;

static void host1x_set_irq_status_bit(uint8_t irq_bit, int cpu_id, bool enable)
{
    uint32_t irq_mask = 1 << irq_bit;
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
        case CPU:
            TRACE_IRQ_SET(0x50003000, *syncpts_irq, CPU_IRQ(enable));
            break;
        case COP:
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
    return host1x_modules_irq_cpu_mask;
}

inline uint32_t host1x_get_modules_irq_cop_mask(void)
{
    return host1x_modules_irq_cop_mask;
}

inline uint32_t host1x_get_syncpts_cpu_irq_status(void)
{
    return host1x_syncpts_cpu_irq_status;
}

inline uint32_t host1x_get_syncpts_cop_irq_status(void)
{
    return host1x_syncpts_cop_irq_status;
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

void host1x_set_modules_cpu_irq_mask(uint32_t mask)
{
    qemu_mutex_lock(&irq_mutex);

    host1x_modules_irq_cpu_mask = mask & MODULES_MASK;

    qemu_mutex_unlock(&irq_mutex);
}

void host1x_set_modules_cop_irq_mask(uint32_t mask)
{
    qemu_mutex_lock(&irq_mutex);

    host1x_modules_irq_cop_mask = mask & MODULES_MASK;

    qemu_mutex_unlock(&irq_mutex);
}

static void host1x_update_cpu_irq(void)
{
    uint32_t i;

    FOREACH_BIT_SET(host1x_syncpts_cpu_irq_status, i, 32) {
        if (host1x_syncpts_dst_mask & (1 << (i * 2))) {
            host1x_set_irq_status_bit(30, CPU, 1);
            return;
        }
    }

    host1x_set_irq_status_bit(30, CPU, 0);
}

void host1x_enable_syncpts_cpu_irq_mask(uint32_t enable_mask)
{
    unsigned i;

    TPRINT("%s: 0x%08X\n", __func__, enable_mask);

    qemu_mutex_lock(&irq_mutex);

    FOREACH_BIT_SET(enable_mask, i, 32)
        host1x_syncpts_dst_mask |= 1 << (i * 2);

    qemu_mutex_unlock(&irq_mutex);
}

static void host1x_update_cop_irq(void)
{
    unsigned i;

    FOREACH_BIT_SET(host1x_syncpts_cop_irq_status, i, 32) {
        if (host1x_syncpts_dst_mask & (1 << (i * 2 + 1))) {
            host1x_set_irq_status_bit(31, COP, 1);
            return;
        }
    }

    host1x_set_irq_status_bit(31, COP, 0);
}

void host1x_enable_syncpts_cop_irq_mask(uint32_t enable_mask)
{
    unsigned i;

    TPRINT("%s: 0x%08X\n", __func__, enable_mask);

    qemu_mutex_lock(&irq_mutex);

    FOREACH_BIT_SET(enable_mask, i, 32)
        host1x_syncpts_dst_mask |= 1 << (i * 2 + 1);

    qemu_mutex_unlock(&irq_mutex);
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

    FOREACH_BIT_SET(clear_mask, i, 32)
        host1x_syncpts_dst_mask &= ~(3 << (i * 2));

    qemu_mutex_unlock(&irq_mutex);
}

void host1x_set_syncpt_irq(uint8_t syncpt_id)
{
    uint32_t syncpt_id_irq_mask = 1 << syncpt_id;

    TPRINT("%s: %d\n", __func__, syncpt_id);

    qemu_mutex_lock(&irq_mutex);

    if (host1x_syncpts_dst_mask & (1 << (syncpt_id * 2))) {
        host1x_syncpts_cpu_irq_status |= syncpt_id_irq_mask;
        host1x_set_irq_status_bit(30, CPU, 1);
    }

    if (host1x_syncpts_dst_mask & (1 << (syncpt_id * 2 + 1))) {
        host1x_syncpts_cop_irq_status |= syncpt_id_irq_mask;
        host1x_set_irq_status_bit(31, COP, 1);
    }

    qemu_mutex_unlock(&irq_mutex);
}

void host1x_clear_syncpts_cpu_irq_status(uint32_t clear_mask)
{
    int i;

    qemu_mutex_lock(&irq_mutex);

    FOREACH_BIT_SET(clear_mask, i, 32) {
        if (!(host1x_syncpts_dst_mask & (1 << (i * 2))))
            continue;

        /* Don't clear if IRQ line is enabled and active.  */
        if (host1x_syncpt_threshold_is_crossed(i))
            clear_mask &= ~(1 << i);
    }

    host1x_syncpts_cpu_irq_status &= ~clear_mask;

    host1x_update_cpu_irq();

    qemu_mutex_unlock(&irq_mutex);
}

void host1x_clear_syncpts_cop_irq_status(uint32_t clear_mask)
{
    int i;

    qemu_mutex_lock(&irq_mutex);

    FOREACH_BIT_SET(clear_mask, i, 32) {
        if (!(host1x_syncpts_dst_mask & (1 << (i * 2 + 1))))
            continue;

        /* Don't clear if IRQ line is enabled and active.  */
        if (host1x_syncpt_threshold_is_crossed(i))
            clear_mask &= ~(1 << i);
    }

    host1x_syncpts_cop_irq_status &= ~clear_mask;

    host1x_update_cop_irq();

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
    host1x_modules_irq_cpu_mask = 0;
    host1x_modules_irq_cop_mask = 0;
    host1x_syncpts_dst_mask = 0;
}
