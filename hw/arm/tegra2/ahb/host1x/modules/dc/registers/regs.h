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

#ifndef TEGRA_DC_REGS_H
#define TEGRA_DC_REGS_H

#include <stdint.h>

#include "qemu/queue.h"

#define DEFINE_REG32(reg) reg##_t reg

enum shadow_type {
    ACTIVE,
    ASSEMBLY,
    ARMED,
};

typedef struct regs_io_handler {
    uint32_t (*read) (void *regs, uint32_t offset);
    void (*write) (void *regs, uint32_t offset, uint32_t value);
    void (*reset) (void *regs);

    uint32_t begin;
    uint32_t end;
} regs_io_handler;

typedef struct win_regs {
    regs_io_handler io_handler;
    enum shadow_type shadow_type;
    size_t regs_sz;

    QLIST_ENTRY(win_regs) next;

    void *assembly;
    void *armed;
    void *active;
} win_regs;

#endif // TEGRA_DC_REGS_H
