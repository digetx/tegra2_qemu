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

#include "host1x_module.h"

#define MODULES_MASK    0xFFF

static uint32_t modules_irq_mask;
static uint32_t modules_percpu_irq_mask[HOST1X_CPUS_NB];

/*
 * This just a stub for now.
 */

inline uint32_t host1x_get_modules_irq_mask(void)
{
    return modules_irq_mask;
}

inline uint32_t host1x_get_modules_irq_cpu_mask(void)
{
    return modules_percpu_irq_mask[HOST1X_CPU];
}

inline uint32_t host1x_get_modules_irq_cop_mask(void)
{
    return modules_percpu_irq_mask[HOST1X_COP];
}

void host1x_set_modules_irq_mask(uint32_t mask)
{
    modules_irq_mask = mask;
}

void host1x_set_modules_percpu_irq_mask(enum hcpu cpu_id, uint32_t mask)
{
    modules_percpu_irq_mask[cpu_id] = mask & MODULES_MASK;
}

void host1x_reset_modules_irqs(void)
{
    enum hcpu cpu_id;

    modules_irq_mask = 0;

    FOREACH_CPU(cpu_id) {
        modules_percpu_irq_mask[cpu_id] = 0;
    }
}
