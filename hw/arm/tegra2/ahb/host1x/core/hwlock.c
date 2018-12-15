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

#include "host1x_priv.h"
#include "host1x_hwlock.h"

static uint32_t hwlock;

int host1x_acquire_hwlock(unsigned lock_nb)
{
    assert(lock_nb < NV_HOST1X_NB_MLOCKS);

    if (hwlock & (1 << lock_nb))
        return 0;

    hwlock |= 1 << lock_nb;

    return 1;
}

void host1x_release_hwlock(unsigned lock_nb)
{
    assert(lock_nb < NV_HOST1X_NB_MLOCKS);

    hwlock &= ~(1 << lock_nb);
}

void host1x_reset_hwlocks(void)
{
    hwlock = 0;
}
