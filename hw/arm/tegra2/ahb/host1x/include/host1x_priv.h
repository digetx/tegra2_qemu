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

#ifndef TEGRA_HOST1X_PRIV_H
#define TEGRA_HOST1X_PRIV_H

#define NV_HOST1X_SYNCPT_THESH_WIDTH    16
#define NV_HOST1X_SYNCPT_NB_BASES       8
#define NV_HOST1X_SYNCPT_NB_PTS         32
#define NV_HOST1X_NB_MLOCKS             16
#define CHANNELS_NB                     8

static inline bool find_set_bit(uint32_t val, uint32_t *itr, uint32_t size)
{
    if (val == 0)
        return false;

    for (; *itr < size; (*itr)++)
        if ((val >> *itr) & 1)
            return true;

    return false;
}

#define FOREACH_BIT_SET(val, itr, size)                 \
    for (itr = 0; find_set_bit(val, &itr, size); itr++)

#endif // TEGRA_HOST1X_PRIV_H
