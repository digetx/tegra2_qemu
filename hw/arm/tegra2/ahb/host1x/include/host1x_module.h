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

#ifndef HOST1X_MODULE_H
#define HOST1X_MODULE_H

#include "qemu/osdep.h"
#include "qemu/queue.h"
#include "qemu/thread.h"

#include "host1x_priv.h"

struct host1x_module {
    uint8_t module_id;
    uint8_t class_id;
    uint8_t owner;
    void *opaque;
    QLIST_ENTRY(host1x_module) next;

    void (*reg_write) (struct host1x_module *module,
                       uint32_t offset, uint32_t value);
    uint32_t (*reg_read) (struct host1x_module *module, uint32_t offset);
};

struct host1x_cdma;

uint32_t host1x_cpu_get_mlock_val(uint32_t id);
uint32_t host1x_cpu_acquire_mlock(uint32_t id);
void host1x_cpu_release_mlock(uint32_t id);
void host1x_ch_acquire_mlock(struct host1x_cdma *cdma, uint32_t id);
void host1x_ch_release_mlock(struct host1x_cdma *cdma, uint32_t id);
void host1x_wake_mlocked_channels(void);
void host1x_reset_mlocks(void);
void host1x_init_mlocks(void);
void register_host1x_bus_module(struct host1x_module* module, void *opaque);
struct host1x_module* get_host1x_module(uint32_t class_id);
uint32_t host1x_module_read(struct host1x_module* module, uint32_t offset);
void host1x_module_write(struct host1x_module* module,
                         uint32_t offset, uint32_t value);

uint32_t host1x_get_modules_irq_mask(void);
uint32_t host1x_get_modules_irq_cpu_mask(void);
uint32_t host1x_get_modules_irq_cop_mask(void);
void host1x_set_modules_irq_mask(uint32_t mask);
void host1x_set_modules_percpu_irq_mask(enum hcpu cpu_id, uint32_t mask);
void host1x_reset_modules_irqs(void);

#endif // HOST1X_MODULE_H
