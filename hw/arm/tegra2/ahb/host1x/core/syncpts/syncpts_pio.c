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
#include "host1x_hwlock.h"

#include "tegra_trace.h"

uint32_t host1x_sync_read_reg(uint32_t addr)
{
#ifdef TEGRA_TRACE
    uint32_t base = addr & 0xFFFFF000;
#endif
    uint32_t offset = addr & 0xFFF;
    uint32_t ret = 0;

    switch (offset) {
    case INTSTATUS_OFFSET:
        ret = host1x_get_syncpts_irq_status();
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
        ret = host1x_get_syncpts_dst_mask_low();
        break;
    case SYNCPT_THRESH_INT_MASK_1_OFFSET:
        ret = host1x_get_syncpts_dst_mask_high();
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
        host1x_set_syncpts_irq_dst_mask(0, value);
        break;
    case SYNCPT_THRESH_INT_MASK_1_OFFSET:
        TRACE_WRITE(base, offset, 0, value);
        host1x_set_syncpts_irq_dst_mask(1, value);
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
