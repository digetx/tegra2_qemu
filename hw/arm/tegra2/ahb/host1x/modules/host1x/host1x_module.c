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

#include "host1x_cdma.h"
#include "host1x_module.h"
#include "host1x.h"

#include "tegra_trace.h"

void host1x_write(struct host1x_module *module, uint32_t offset, uint32_t data)
{
    struct host1x_syncpt_waiter *waiter = &host1x_cdma_ptr->waiter;

    TRACE_WRITE(module->class_id, offset, data, data);

    switch (offset) {
    case NV_CLASS_HOST_INCR_SYNCPT_OFFSET:
    {
        nv_class_host_incr_syncpt method = { .reg32 = data };

        host1x_incr_syncpt(method.indx);
        break;
    }
    case NV_CLASS_HOST_INCR_SYNCPT_CNTRL_OFFSET:
    {
//         nv_class_host_incr_syncpt_cntrl method = { .reg32 = data };
        break;
    }
    case NV_CLASS_HOST_INCR_SYNCPT_ERROR_OFFSET:
    {
//         nv_class_host_incr_syncpt_error method = { .reg32 = data };
        break;
    }
    case NV_CLASS_HOST_WAIT_SYNCPT_OFFSET:
    {
        nv_class_host_wait_syncpt method = { .reg32 = data };

        host1x_wait_syncpt(waiter, method.indx, method.thresh);
        break;
    }
    case NV_CLASS_HOST_WAIT_SYNCPT_BASE_OFFSET:
    {
        nv_class_host_wait_syncpt_base method = { .reg32 = data };

        host1x_wait_syncpt_base(waiter, method.indx, method.base_indx,
                                method.offset);
        break;
    }
    case NV_CLASS_HOST_WAIT_SYNCPT_INCR_OFFSET:
    {
        nv_class_host_wait_syncpt_incr method = { .reg32 = data };

        host1x_wait_syncpt_incr(waiter, method.indx);
        break;
    }
    case NV_CLASS_HOST_LOAD_SYNCPT_BASE_OFFSET:
    {
        nv_class_host_load_syncpt_base method = { .reg32 = data };

        host1x_set_syncpt_base(method.base_indx, method.value);
        break;
    }
    case NV_CLASS_HOST_INCR_SYNCPT_BASE_OFFSET:
    {
        nv_class_host_incr_syncpt_base method = { .reg32 = data };

        host1x_incr_syncpt_base(method.base_indx, method.offset);
        break;
    }
    case NV_CLASS_HOST_DELAY_USEC_OFFSET:
    {
//         nv_class_host_delay_usec method = { .reg32 = data };
        break;
    }
    case NV_CLASS_HOST_TICKCOUNT_HI_OFFSET:
    {
//         nv_class_host_tickcount_hi method = { .reg32 = data };
        break;
    }
    case NV_CLASS_HOST_TICKCOUNT_LO_OFFSET:
    {
//         nv_class_host_tickcount_lo method = { .reg32 = data };
        break;
    }
    case NV_CLASS_HOST_TICKCTRL_OFFSET:
    {
//         nv_class_host_tickctrl method = { .reg32 = data };
        break;
    }
    case NV_CLASS_HOST_INDCTRL_OFFSET:
    {
//         nv_class_host_indctrl method = { .reg32 = data };

        g_assert_not_reached();
        break;
    }
    case NV_CLASS_HOST_INDOFF2_OFFSET:
    {
//         nv_class_host_indoff2 method = { .reg32 = data };

//         g_assert_not_reached();
        break;
    }
    case NV_CLASS_HOST_INDOFF_OFFSET:
    {
//         nv_class_host_indoff method = { .reg32 = data };

//         g_assert_not_reached();
        break;
    }
    case NV_CLASS_HOST_INDDATA_OFFSET_BEGIN ... NV_CLASS_HOST_INDDATA_OFFSET_END:
    {
        g_assert_not_reached();
        break;
    }
    default:
        g_assert_not_reached();
        break;
    }
}

uint32_t host1x_read(struct host1x_module *module, uint32_t offset)
{
    return 0;
}
