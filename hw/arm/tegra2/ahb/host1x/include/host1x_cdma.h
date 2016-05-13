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

#ifndef TEGRA_HOST1X_CDMA_H
#define TEGRA_HOST1X_CDMA_H

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "sysemu/dma.h"

#include "host1x_syncpts.h"

extern void *host1x_dma_ptr;

/* TODO: get rid of it*/
extern __thread struct host1x_cdma *host1x_cdma_ptr;

struct host1x_dma_gather {
    struct host1x_cdma *cdma;
    uint32_t base:30;
    uint32_t get:30;
    uint32_t put:30;
    bool inlined:1;
};

struct host1x_cdma {
    struct host1x_syncpt_waiter waiter;
    struct host1x_dma_gather gather;
    struct host1x_module *module;
    QemuEvent start_ev;
    QemuEvent stop_ev;
    QemuThread thread;
    uint8_t ch_id;
    uint32_t base:30;
    uint32_t end:30;
    bool enabled:1;
};

void host1x_init_dma(void);
void host1x_cdma_set_base(struct host1x_cdma *cdma, uint32_t base);
void host1x_cdma_set_put(struct host1x_cdma *cdma, uint32_t put);
void host1x_cdma_set_end(struct host1x_cdma *cdma, uint32_t end);
void host1x_cdma_control(struct host1x_cdma *cdma,
                         bool dma_stop, bool dma_get_rst, bool dma_init_get);
void host1x_init_cdma(struct host1x_cdma *cdma, uint8_t ch_id);

#endif // TEGRA_HOST1X_CDMA_H
