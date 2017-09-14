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

#include "host1x_cdma.h"
#include "host1x_module.h"
#include "host1x_priv.h"

typedef union host1x_mlock {
    struct {
        unsigned int ch_owns:1;
        unsigned int cpu_owns:1;
        unsigned int undefined_bits_1:5;
        unsigned int owner_chid:4;
        unsigned int undefined_bits_2:20;
        QemuMutex mutex;
        QemuCond release_cond;
    };

    uint32_t reg32;
} host1x_mlock;

static host1x_mlock mlocks[NV_HOST1X_NB_MLOCKS];

uint32_t host1x_cpu_get_mlock_val(uint32_t id)
{
    host1x_mlock *mlock = &mlocks[id];

    g_assert(id < NV_HOST1X_NB_MLOCKS);

    return mlock->reg32;
}

uint32_t host1x_cpu_acquire_mlock(uint32_t id)
{
    host1x_mlock *mlock = &mlocks[id];

    g_assert(id < NV_HOST1X_NB_MLOCKS);

    qemu_mutex_lock(&mlock->mutex);

    if (!mlock->ch_owns)
        mlock->cpu_owns = 1;

    qemu_mutex_unlock(&mlock->mutex);

    return !mlock->cpu_owns;
}

void host1x_cpu_release_mlock(uint32_t id)
{
    host1x_mlock *mlock = &mlocks[id];

    g_assert(id < NV_HOST1X_NB_MLOCKS);

    qemu_mutex_lock(&mlock->mutex);

    mlock->ch_owns = 0;
    mlock->cpu_owns = 0;
    qemu_cond_signal(&mlock->release_cond);

    qemu_mutex_unlock(&mlock->mutex);
}

void host1x_ch_acquire_mlock(struct host1x_cdma *cdma, uint32_t id)
{
    host1x_mlock *mlock = &mlocks[id];

    g_assert(id < NV_HOST1X_NB_MLOCKS);

    qemu_mutex_lock(&mlock->mutex);

    while ((mlock->cpu_owns || mlock->ch_owns) && cdma->enabled)
        qemu_cond_wait(&mlock->release_cond, &mlock->mutex);

    mlock->owner_chid = cdma->ch_id;
    mlock->ch_owns = 1;

    qemu_mutex_unlock(&mlock->mutex);
}

void host1x_ch_release_mlock(struct host1x_cdma *cdma, uint32_t id)
{
    host1x_mlock *mlock = &mlocks[id];

    g_assert(id < NV_HOST1X_NB_MLOCKS);

    qemu_mutex_lock(&mlock->mutex);

    while (cdma->enabled &&
                (mlock->cpu_owns ||
                        (mlock->ch_owns && mlock->owner_chid != cdma->ch_id)))
        qemu_cond_wait(&mlock->release_cond, &mlock->mutex);

    mlock->ch_owns = 0;
    qemu_cond_signal(&mlock->release_cond);

    qemu_mutex_unlock(&mlock->mutex);
}

void host1x_wake_mlocked_channels(void)
{
    host1x_mlock *mlock;
    int i;

    for (i = 0; i < NV_HOST1X_NB_MLOCKS; i++) {
        mlock = &mlocks[i];

        qemu_mutex_lock(&mlock->mutex);

        qemu_cond_signal(&mlock->release_cond);

        qemu_mutex_unlock(&mlock->mutex);
    }
}

void host1x_reset_mlocks(void)
{
    int i;

    for (i = 0; i < NV_HOST1X_NB_MLOCKS; i++)
        mlocks[i].reg32 = 0;
}

void host1x_init_mlocks(void)
{
    int i;

    for (i = 0; i < NV_HOST1X_NB_MLOCKS; i++) {
        qemu_mutex_init(&mlocks[i].mutex);
        qemu_cond_init(&mlocks[i].release_cond);
    }
}
