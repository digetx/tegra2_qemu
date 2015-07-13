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

#include <assert.h>

#include "qemu/thread.h"

#include "fifo.h"

void fifo_init(struct host1x_fifo *fifo)
{
    fifo->entries_nb = 0;
    fifo->first = 0;
    fifo->last = 0;
    qemu_mutex_init(&fifo->mutex);
    qemu_cond_init(&fifo->free_cond);
}

uint8_t get_fifo_entries_nb(struct host1x_fifo *fifo)
{
    return fifo->entries_nb;
}

void fifo_push(struct host1x_fifo *fifo, uint32_t data)
{
    qemu_mutex_lock(&fifo->mutex);

    if (fifo->entries_nb == (FIFO_SIZE - 1))
        qemu_cond_wait(&fifo->free_cond, &fifo->mutex);

    fifo->data[fifo->last++] = data;
    fifo->entries_nb++;

    qemu_mutex_unlock(&fifo->mutex);
}

uint32_t fifo_pop(struct host1x_fifo *fifo)
{
    uint32_t ret;

    qemu_mutex_lock(&fifo->mutex);

    assert(fifo->entries_nb != 0);

    ret = fifo->data[fifo->first++];
    fifo->entries_nb--;

    qemu_cond_signal(&fifo->free_cond);

    qemu_mutex_unlock(&fifo->mutex);

    return ret;
}
