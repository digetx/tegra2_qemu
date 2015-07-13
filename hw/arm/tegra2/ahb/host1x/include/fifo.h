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

#ifndef TEGRA_HOST1X_FIFO_H
#define TEGRA_HOST1X_FIFO_H

#define FIFO_SIZE   32

struct host1x_fifo {
    uint32_t data[FIFO_SIZE];
    unsigned int entries_nb:5;
    unsigned int first:5;
    unsigned int last:5;
    QemuMutex mutex;
    QemuCond free_cond;
};

uint8_t get_fifo_entries_nb(struct host1x_fifo *fifo);
void fifo_push(struct host1x_fifo *fifo, uint32_t data);
uint32_t fifo_pop(struct host1x_fifo *fifo);
void fifo_init(struct host1x_fifo *fifo);

#endif // TEGRA_HOST1X_FIFO_H
