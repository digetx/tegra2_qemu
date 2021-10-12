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

#ifndef HOST1X_CORE_SYNCPTS_H
#define HOST1X_CORE_SYNCPTS_H

#include <stdint.h>

#include "qemu/thread.h"

#define syncpt_lock(s)      qemu_mutex_lock(&s->mutex)
#define syncpt_unlock(s)    qemu_mutex_unlock(&s->mutex)

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

extern struct host1x_syncpt syncpts[];
extern struct host1x_syncpt_base syncpt_bases[];

int counter_reached_threshold(uint32_t counter, uint32_t threshold);
void host1x_update_threshold_waiters(struct host1x_syncpt *syncpt);
void host1x_update_threshold_waiters_base(uint32_t syncpt_base_id);

#endif // HOST1X_CORE_SYNCPTS_H
