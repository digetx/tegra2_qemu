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

#include "hw/sysbus.h"

#include "host1x_module.h"
#include "host1x_syncpts.h"

#include "tegra_trace.h"

#define SYNCPT_TMASK(s) ((s) & (BIT_MASK(NV_HOST1X_SYNCPT_THESH_WIDTH) - 1))

struct host1x_syncpt {
    uint32_t counter;
    unsigned int threshold:NV_HOST1X_SYNCPT_THESH_WIDTH;
    QLIST_HEAD(, host1x_syncpt_waiter) waiters;
    QLIST_HEAD(, host1x_syncpt_waiter) waiters_base;
    QLIST_HEAD(, host1x_syncpt_waiter) waiters_incr;
    QemuMutex mutex;
};

struct host1x_syncpt_base {
    uint32_t base;
    QemuMutex mutex;
};

static struct host1x_syncpt syncpts[NV_HOST1X_SYNCPT_NB_PTS];
static struct host1x_syncpt_base syncpt_bases[NV_HOST1X_SYNCPT_NB_BASES];

static uint8_t hwlock;

static uint32_t host1x_acquire_hwlock(uint8_t lock_nb)
{
    if (hwlock & (1 << lock_nb))
        return 0;

    hwlock |= 1 << lock_nb;

    return 1;
}

static void host1x_release_hwlock(uint8_t lock_nb)
{
    hwlock &= ~(1 << lock_nb);
}

uint32_t host1x_sync_read_reg(uint32_t addr)
{
#ifdef TEGRA_TRACE
    uint32_t base = addr & 0xFFFFF000;
#endif
    uint32_t offset = addr & 0xFFF;
    uint32_t ret = 0;

    switch (offset) {
    case INTSTATUS_OFFSET:
        ret = host1x_get_sync_irq_status();
        break;
    case INTMASK_OFFSET:
        ret = host1x_get_modules_irq_mask();
        break;
    case INTC0MASK_OFFSET:
        ret = host1x_get_modules_irq_cpu_mask();
        break;
    case INTC1MASK_OFFSET:
        ret = host1x_get_modules_irq_cop_mask();
        break;
    case HINTSTATUS_OFFSET:
        break;
    case HINTMASK_OFFSET:
        break;
    case HINTSTATUS_EXT_OFFSET:
        break;
    case HINTMASK_EXT_OFFSET:
        break;
    case SYNCPT_THRESH_CPU0_INT_STATUS_OFFSET:
        ret = host1x_get_syncpts_cpu_irq_status();
        break;
    case SYNCPT_THRESH_CPU1_INT_STATUS_OFFSET:
        ret = host1x_get_syncpts_cop_irq_status();
        break;
    case SYNCPT_THRESH_INT_MASK_OFFSET:
        ret = host1x_get_syncpts_dst_mask_high();
        break;
    case SYNCPT_THRESH_INT_MASK_1_OFFSET:
        ret = host1x_get_syncpts_dst_mask_low();
        break;
    case SYNCPT_THRESH_INT_DISABLE_OFFSET:
        break;
    case SYNCPT_THRESH_INT_ENABLE_CPU0_OFFSET:
        break;
    case SYNCPT_THRESH_INT_ENABLE_CPU1_OFFSET:
        break;
    case CF0_SETUP_OFFSET ... CF7_SETUP_OFFSET:
        break;
    case CF_SETUPDONE_OFFSET:
        break;
    case CMDPROC_CTRL_OFFSET:
        break;
    case CMDPROC_STAT_OFFSET:
        break;
    case CMDPROC_STOP_OFFSET:
        break;
    case CH_TEARDOWN_OFFSET:
        break;
    case MOD_TEARDOWN_OFFSET:
        break;
    case CH0_STATUS_OFFSET ... CH7_STATUS_OFFSET:
        break;
    case DISPLAY_STATUS_OFFSET:
        break;
    case DISPLAYB_STATUS_OFFSET:
        break;
    case EPP_STATUS_OFFSET:
        break;
    case GR3D_STATUS_OFFSET:
        break;
    case ISP_STATUS_OFFSET:
        break;
    case MPE_STATUS_OFFSET:
        break;
    case TVO_STATUS_OFFSET:
        break;
    case DSI_STATUS_OFFSET:
        break;
    case HDMI_STATUS_OFFSET:
        break;
    case VI_STATUS_OFFSET:
        break;
    case GR2D_STATUS_OFFSET:
        break;
    case DIRECT_MODULE_CONFIG_OFFSET:
        break;
    case USEC_CLK_OFFSET:
        break;
    case CTXSW_TIMEOUT_CFG_OFFSET:
        break;
    case INDREG_DMA_CTRL_OFFSET:
        break;
    case CHANNEL_PRIORITY_OFFSET:
        break;
    case CDMA_ASM_TIMEOUT_OFFSET:
        break;
    case CDMA_MISC_OFFSET:
        break;
    case IP_BUSY_TIMEOUT_OFFSET:
        break;
    case IP_READ_TIMEOUT_ADDR_OFFSET:
        break;
    case IP_WRITE_TIMEOUT_ADDR_OFFSET:
        break;
    case MCCIF_THCTRL_OFFSET:
        break;
    case HC_MCCIF_FIFOCTRL_OFFSET:
        break;
    case TIMEOUT_WCOAL_HC_OFFSET:
        break;
    case HWLOCK0_OFFSET ... HWLOCK7_OFFSET:
        ret = host1x_acquire_hwlock((offset & 0xf) >> 2);
        break;
    case MLOCK_OFFSET ... MLOCK_15_OFFSET:
        ret = host1x_cpu_acquire_mlock((offset & 0x3f) >> 2);
        break;
    case MLOCK_OWNER_OFFSET ... MLOCK_OWNER_15_OFFSET:
        ret = host1x_cpu_get_mlock_val((offset & 0x3f) >> 2);
        break;
    case MLOCK_ERROR_OFFSET:
        break;
    case SYNCPT_OFFSET ... SYNCPT_31_OFFSET:
        ret = host1x_get_syncpt_count((offset & 0xff) >> 2);
        break;
    case SYNCPT_INT_THRESH_OFFSET ... SYNCPT_INT_THRESH_31_OFFSET:
        ret = host1x_get_syncpt_threshold((offset & 0xff) >> 2);
        break;
    case SYNCPT_BASE_OFFSET ... SYNCPT_BASE_7_OFFSET:
        ret = host1x_get_syncpt_base((offset & 0xf) >> 2);
        break;
    case SYNCPT_CPU_INCR_OFFSET:
        break;
    case CBREAD0_OFFSET ... CBREAD7_OFFSET:
        break;
    case REGF_DATA_OFFSET:
        break;
    case REGF_ADDR_OFFSET:
        break;
    case WAITOVR_OFFSET:
        break;
    case CFPEEK_CTRL_OFFSET:
        break;
    case CFPEEK_READ_OFFSET:
        break;
    case CFPEEK_PTRS_OFFSET:
        break;
    case CBSTAT0_OFFSET ... CBSTAT7_OFFSET:
        break;
    case CDMA_STATS_WORDS_FETCHED_OFFSET:
        break;
    case CDMA_STATS_WORDS_DISCARDED_OFFSET:
        break;
    case CFG_OFFSET:
        break;
    case RDMA_MISC_OFFSET:
        break;
    case RDMA_ARB_COUNT_OFFSET:
        break;
    case RDMA_CONFIG_OFFSET:
        break;
    case RDMA_WRAP_OFFSET:
        break;
    case RDMA_STATUS0_OFFSET:
        break;
    case RDMA_BUFFER_THRESHOLD0_OFFSET:
        break;
    case RDMA_CONF0_OFFSET:
        break;
    case RDMA_SWAP0_OFFSET:
        break;
    case RDMA_LINE0_OFFSET:
        break;
    case RDMA_CLID0_OFFSET:
        break;
    case RDMA_BADDR0_OFFSET:
        break;
    case RDMA_DMATRIGGER0_OFFSET:
        break;
    default:
        break;
    }

    TRACE_READ(base, offset, ret);

    return ret;
}

void host1x_sync_write_reg(uint32_t addr, uint32_t value)
{
#ifdef TEGRA_TRACE
    uint32_t base = addr & 0xFFFFF000;
#endif
    uint32_t offset = addr & 0xFFF;
    unsigned i;

    switch (offset) {
    case INTMASK_OFFSET:
        TRACE_WRITE(base, offset, host1x_get_modules_irq_mask(), value);
        host1x_set_modules_irq_mask(value);
        break;
    case INTC0MASK_OFFSET:
        TRACE_WRITE(base, offset, host1x_get_modules_irq_cpu_mask(), value);
        host1x_set_modules_percpu_irq_mask(HOST1X_CPU, value);
        break;
    case INTC1MASK_OFFSET:
        TRACE_WRITE(base, offset, host1x_get_modules_irq_cop_mask(), value);
        host1x_set_modules_percpu_irq_mask(HOST1X_COP, value);
        break;
    case HINTSTATUS_OFFSET:
        TRACE_WRITE(base, offset, 0, value & HINTSTATUS_WRMASK);
        break;
    case HINTMASK_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case HINTSTATUS_EXT_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case HINTMASK_EXT_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case SYNCPT_THRESH_CPU0_INT_STATUS_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        host1x_clear_syncpts_irq_status(HOST1X_CPU, value);
        break;
    case SYNCPT_THRESH_CPU1_INT_STATUS_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        host1x_clear_syncpts_irq_status(HOST1X_COP, value);
        break;
    case SYNCPT_THRESH_INT_MASK_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        host1x_set_syncpts_irq_dst_mask(value, 1);
        break;
    case SYNCPT_THRESH_INT_MASK_1_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        host1x_set_syncpts_irq_dst_mask(value, 0);
        break;
    case SYNCPT_THRESH_INT_DISABLE_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        host1x_clear_syncpts_irq_dst_mask(value);
        break;
    case SYNCPT_THRESH_INT_ENABLE_CPU0_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        host1x_enable_syncpts_irq_mask(HOST1X_CPU, value);
        break;
    case SYNCPT_THRESH_INT_ENABLE_CPU1_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        host1x_enable_syncpts_irq_mask(HOST1X_COP, value);
        break;
    case CF0_SETUP_OFFSET ... CF7_SETUP_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case CF_SETUPDONE_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case CMDPROC_CTRL_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case CMDPROC_STOP_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case CH_TEARDOWN_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case MOD_TEARDOWN_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case DIRECT_MODULE_CONFIG_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case USEC_CLK_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case CTXSW_TIMEOUT_CFG_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case INDREG_DMA_CTRL_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case CHANNEL_PRIORITY_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case CDMA_ASM_TIMEOUT_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case CDMA_MISC_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case IP_BUSY_TIMEOUT_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case MCCIF_THCTRL_OFFSET:
        TRACE_WRITE(base, offset, 0, value & MCCIF_THCTRL_WRMASK);
        break;
    case HC_MCCIF_FIFOCTRL_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case TIMEOUT_WCOAL_HC_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case HWLOCK0_OFFSET ... HWLOCK7_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        host1x_release_hwlock((offset & 0xf) >> 2);
        break;
    case MLOCK_OFFSET ... MLOCK_15_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        host1x_cpu_release_mlock((offset & 0x3f) >> 2);
        break;
    case MLOCK_ERROR_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case SYNCPT_OFFSET ... SYNCPT_31_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        host1x_set_syncpt_count((offset & 0xff) >> 2, value);
        break;
    case SYNCPT_INT_THRESH_OFFSET ... SYNCPT_INT_THRESH_31_OFFSET:
    {
        syncpt_int_thresh_t thresh = { .reg32 = value };
        TRACE_WRITE(base, offset, 0, value);
        host1x_set_syncpt_threshold((offset & 0xff) >> 2, thresh.int_thresh);
        break;
    }
    case SYNCPT_BASE_OFFSET ... SYNCPT_BASE_7_OFFSET:
    {
        syncpt_base_t syncpt_base = { .reg32 = value };
        TRACE_WRITE(base, offset, 0, value);
        host1x_set_syncpt_base((offset & 0xf) >> 2, syncpt_base.base_0);
        break;
    }
    case SYNCPT_CPU_INCR_OFFSET:
        TRACE_WRITE(base, offset, 0, value);

        FOREACH_BIT_SET(value, i, NV_HOST1X_SYNCPT_NB_PTS)
            host1x_incr_syncpt(i);
        break;
    case WAITOVR_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case CFPEEK_CTRL_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case CFG_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case RDMA_MISC_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case RDMA_ARB_COUNT_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case RDMA_CONFIG_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case RDMA_WRAP_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case RDMA_STATUS0_OFFSET:
        TRACE_WRITE(base, offset, 0, value & RDMA_STATUS0_WRMASK);
        break;
    case RDMA_BUFFER_THRESHOLD0_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case RDMA_CONF0_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case RDMA_SWAP0_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case RDMA_LINE0_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case RDMA_CLID0_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case RDMA_BADDR0_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    case RDMA_DMATRIGGER0_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        break;
    default:
        TRACE_WRITE(base, offset, 0, value);
        break;
    }
}

void host1x_unlock_syncpt_waiter(struct host1x_syncpt_waiter *waiter)
{
    QLIST_REMOVE(waiter, next);
    qemu_event_set(&waiter->syncpt_ev);
}

void host1x_unlock_syncpt_waiter_forced(struct host1x_syncpt_waiter *waiter)
{
    struct host1x_syncpt_waiter *waiter__, *waiter_next;
    int i;

    for (i = 0; i < NV_HOST1X_SYNCPT_NB_PTS; i++) {
        struct host1x_syncpt *syncpt = &syncpts[i];

        qemu_mutex_lock(&syncpt->mutex);

        QLIST_FOREACH_SAFE(waiter__, &syncpt->waiters, next, waiter_next) {
            if (waiter != waiter__)
                continue;

            host1x_unlock_syncpt_waiter(waiter);
            qemu_mutex_unlock(&syncpt->mutex);
            return;
        }

        QLIST_FOREACH_SAFE(waiter__, &syncpt->waiters_base, next, waiter_next) {
            if (waiter != waiter__)
                continue;

            host1x_unlock_syncpt_waiter(waiter);
            qemu_mutex_unlock(&syncpt->mutex);
            return;
        }

        QLIST_FOREACH_SAFE(waiter__, &syncpt->waiters_incr, next, waiter_next) {
            if (waiter != waiter__)
                continue;

            host1x_unlock_syncpt_waiter(waiter);
            qemu_mutex_unlock(&syncpt->mutex);
            return;
        }

        qemu_mutex_unlock(&syncpt->mutex);
    }
}

/* Host1x uses HW optimization for syncpt/threshold comparison.  */
static int syncpt_reached_threshold(uint32_t counter, uint32_t threshold)
{
    uint32_t sub = threshold - counter;
    int overflow = !!(sub & (1 << (NV_HOST1X_SYNCPT_THESH_WIDTH - 1)));
    int eq = !(sub & ((1 << (NV_HOST1X_SYNCPT_THESH_WIDTH - 1)) - 1));

    return ((overflow && !eq) || (!overflow && eq));
}

static void host1x_update_threshold_waiters(uint32_t syncpt_id,
                                            struct host1x_syncpt *syncpt)
{
    struct host1x_syncpt_waiter *waiter, *waiter_next;

    if (syncpt_reached_threshold(syncpt->counter, syncpt->threshold))
        host1x_set_syncpt_irq(syncpt_id);

    QLIST_FOREACH_SAFE(waiter, &syncpt->waiters, next, waiter_next) {
        if (syncpt_reached_threshold(syncpt->counter, waiter->threshold))
            host1x_unlock_syncpt_waiter(waiter);
    }

    QLIST_FOREACH_SAFE(waiter, &syncpt->waiters_base, next, waiter_next) {
        struct host1x_syncpt_base *syncpt_base = &syncpt_bases[waiter->base_id];
        uint32_t threshold;

        qemu_mutex_lock(&syncpt_base->mutex);

        threshold = syncpt_base->base + waiter->threshold;

        if (syncpt_reached_threshold(syncpt->counter, threshold))
            host1x_unlock_syncpt_waiter(waiter);

        qemu_mutex_unlock(&syncpt_base->mutex);
    }
}

void host1x_incr_syncpt(uint32_t syncpt_id)
{
    struct host1x_syncpt *syncpt = &syncpts[syncpt_id];
    struct host1x_syncpt_waiter *waiter, *waiter_next;

    g_assert(syncpt_id < NV_HOST1X_SYNCPT_NB_PTS);

    qemu_mutex_lock(&syncpt->mutex);

    syncpt->counter++;

    TPRINT("%s: id=%d counter=%d counter_masked=%d\n", __func__, syncpt_id,
           syncpt->counter, SYNCPT_TMASK(syncpt->counter));

    host1x_update_threshold_waiters(syncpt_id, syncpt);

    QLIST_FOREACH_SAFE(waiter, &syncpt->waiters_incr, next, waiter_next)
            host1x_unlock_syncpt_waiter(waiter);

    qemu_mutex_unlock(&syncpt->mutex);
}

void host1x_set_syncpt_count(uint32_t syncpt_id, uint32_t val)
{
    struct host1x_syncpt *syncpt = &syncpts[syncpt_id];

    g_assert(syncpt_id < NV_HOST1X_SYNCPT_NB_PTS);

    qemu_mutex_lock(&syncpt->mutex);
    syncpts[syncpt_id].counter = val;
    host1x_update_threshold_waiters(syncpt_id, syncpt);
    qemu_mutex_unlock(&syncpt->mutex);
}

uint32_t host1x_get_syncpt_count(uint32_t syncpt_id)
{
    struct host1x_syncpt *syncpt = &syncpts[syncpt_id];

    g_assert(syncpt_id < NV_HOST1X_SYNCPT_NB_PTS);

    return syncpt->counter;
}

void host1x_set_syncpt_threshold(uint32_t syncpt_id, uint32_t val)
{
    struct host1x_syncpt *syncpt = &syncpts[syncpt_id];

    g_assert(syncpt_id < NV_HOST1X_SYNCPT_NB_PTS);

    qemu_mutex_lock(&syncpt->mutex);
    syncpts[syncpt_id].threshold = val;
    host1x_update_threshold_waiters(syncpt_id, syncpt);
    qemu_mutex_unlock(&syncpt->mutex);
}

uint32_t host1x_get_syncpt_threshold(uint32_t syncpt_id)
{
    struct host1x_syncpt *syncpt = &syncpts[syncpt_id];

    g_assert(syncpt_id < NV_HOST1X_SYNCPT_NB_PTS);

    return syncpt->threshold;
}

static void host1x_update_threshold_waiters_base(uint32_t syncpt_base_id)
{
    struct host1x_syncpt_base *syncpt_base = &syncpt_bases[syncpt_base_id];
    int i;

    for (i = 0; i < ARRAY_SIZE(syncpts); i++) {
        struct host1x_syncpt *syncpt = &syncpts[i];
        struct host1x_syncpt_waiter *waiter, *waiter_next;

        qemu_mutex_lock(&syncpt->mutex);

        QLIST_FOREACH_SAFE(waiter, &syncpt->waiters_base, next, waiter_next) {
            uint32_t threshold;

            if (waiter->base_id != syncpt_base_id)
                continue;

            qemu_mutex_lock(&syncpt_base->mutex);

            threshold = syncpt_base->base + waiter->threshold;

            if (syncpt_reached_threshold(syncpt->counter, threshold))
                host1x_unlock_syncpt_waiter(waiter);

            qemu_mutex_unlock(&syncpt_base->mutex);
        }

        qemu_mutex_unlock(&syncpt->mutex);
    }
}

void host1x_set_syncpt_base(uint32_t syncpt_base_id, uint32_t val)
{
    struct host1x_syncpt_base *syncpt_base = &syncpt_bases[syncpt_base_id];

    g_assert(syncpt_base_id < NV_HOST1X_SYNCPT_NB_BASES);

    qemu_mutex_lock(&syncpt_base->mutex);
    syncpt_base->base = val;
    qemu_mutex_unlock(&syncpt_base->mutex);

    host1x_update_threshold_waiters_base(syncpt_base_id);
}

void host1x_incr_syncpt_base(uint32_t syncpt_base_id, uint32_t val)
{
    struct host1x_syncpt_base *syncpt_base = &syncpt_bases[syncpt_base_id];

    g_assert(syncpt_base_id < NV_HOST1X_SYNCPT_NB_BASES);

    qemu_mutex_lock(&syncpt_base->mutex);
    syncpt_base->base += val;
    qemu_mutex_unlock(&syncpt_base->mutex);

    host1x_update_threshold_waiters_base(syncpt_base_id);
}

uint32_t host1x_get_syncpt_base(uint32_t syncpt_base_id)
{
    struct host1x_syncpt_base *syncpt_base = &syncpt_bases[syncpt_base_id];

    g_assert(syncpt_base_id < NV_HOST1X_SYNCPT_NB_BASES);

    return syncpt_base->base;
}

void host1x_init_syncpt_waiter(struct host1x_syncpt_waiter *waiter)
{
    qemu_event_init(&waiter->syncpt_ev, 1);
}

void host1x_wait_syncpt(struct host1x_syncpt_waiter *waiter,
                        uint32_t syncpt_id, uint32_t threshold)
{
    struct host1x_syncpt *syncpt = &syncpts[syncpt_id];

    g_assert(syncpt_id < NV_HOST1X_SYNCPT_NB_PTS);
//     g_assert(!(threshold & ~BIT_MASK(NV_HOST1X_SYNCPT_THESH_WIDTH)));

    qemu_mutex_lock(&syncpt->mutex);

    waiter->threshold = threshold;

//     if ((threshold & ~BIT_MASK(NV_HOST1X_SYNCPT_THESH_WIDTH)))
//         printf("syncpt_id %d threshold=%d overflow waiter->threshold=%d syncpt->counter=%d\n",
//                syncpt_id, threshold, waiter->threshold, SYNCPT_TMASK(syncpt->counter));

    if (!syncpt_reached_threshold(syncpt->counter, waiter->threshold)) {
        QLIST_INSERT_HEAD(&syncpt->waiters, waiter, next);
        qemu_event_reset(&waiter->syncpt_ev);
    }

    qemu_mutex_unlock(&syncpt->mutex);

    qemu_event_wait(&waiter->syncpt_ev);
}

void host1x_wait_syncpt_base(struct host1x_syncpt_waiter *waiter,
                             uint32_t syncpt_id, uint32_t syncpt_base_id,
                             uint32_t offset)
{
    struct host1x_syncpt_base *syncpt_base = &syncpt_bases[syncpt_base_id];
    struct host1x_syncpt *syncpt = &syncpts[syncpt_id];
    uint32_t threshold;

    g_assert(syncpt_id < NV_HOST1X_SYNCPT_NB_PTS);
    g_assert(syncpt_base_id < NV_HOST1X_SYNCPT_NB_BASES);
//     g_assert(!(offset & ~BIT_MASK(NV_HOST1X_SYNCPT_THESH_WIDTH)));

    qemu_mutex_lock(&syncpt->mutex);

    waiter->threshold = offset;
    waiter->base_id = syncpt_base_id;

    threshold = syncpt_base->base + waiter->threshold;

//     TPRINT("%s: threshold=%d syncpt=%d\n", __func__, threshold,
//            SYNCPT_TMASK(syncpt->counter));

    if (!syncpt_reached_threshold(syncpt->counter, threshold)) {
        QLIST_INSERT_HEAD(&syncpt->waiters_base, waiter, next);
        qemu_event_reset(&waiter->syncpt_ev);
    }

    qemu_mutex_unlock(&syncpt->mutex);

    qemu_event_wait(&waiter->syncpt_ev);
}

void host1x_wait_syncpt_incr(struct host1x_syncpt_waiter *waiter,
                             uint32_t syncpt_id)
{
    struct host1x_syncpt *syncpt = &syncpts[syncpt_id];

    g_assert(syncpt_id < NV_HOST1X_SYNCPT_NB_PTS);

    qemu_mutex_lock(&syncpt->mutex);

    QLIST_INSERT_HEAD(&syncpt->waiters_incr, waiter, next);
    qemu_event_reset(&waiter->syncpt_ev);

    qemu_mutex_unlock(&syncpt->mutex);

    qemu_event_wait(&waiter->syncpt_ev);
}

void host1x_init_syncpts(void)
{
    int i;

    for (i = 0; i < NV_HOST1X_SYNCPT_NB_PTS; i++)
        qemu_mutex_init(&syncpts[i].mutex);

    for (i = 0; i < NV_HOST1X_SYNCPT_NB_BASES; i++)
        qemu_mutex_init(&syncpt_bases[i].mutex);
}

static void host1x_clear_syncpt(struct host1x_syncpt *syncpt)
{
    syncpts->counter = syncpts->threshold = 0;
    QLIST_INIT(&syncpts->waiters);
    QLIST_INIT(&syncpts->waiters_base);
    QLIST_INIT(&syncpts->waiters_incr);
}

/* Channels must be teared down first */
void host1x_reset_syncpts(void)
{
    int i;

    for (i = 0; i < NV_HOST1X_SYNCPT_NB_PTS; i++)
        host1x_clear_syncpt(&syncpts[i]);

    for (i = 0; i < NV_HOST1X_SYNCPT_NB_BASES; i++)
        syncpt_bases[i].base = 0;

    hwlock = 0;
}

int host1x_syncpt_threshold_is_crossed(uint32_t syncpt_id)
{
    struct host1x_syncpt *syncpt = &syncpts[syncpt_id];

    g_assert(syncpt_id < NV_HOST1X_SYNCPT_NB_PTS);

    return syncpt_reached_threshold(syncpt->counter, syncpt->threshold);
}
