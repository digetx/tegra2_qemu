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

#define TEGRA_TRACE

#ifdef TEGRA_TRACE

/* WARNING: HACK */
#include "qom/object.h"
#include "hw/irq.h"
struct IRQState {
    Object parent_obj;

    qemu_irq_handler handler;
    void *opaque;
    int n;
};

#define TRACE_READ(a, o, v) tegra_trace_write(a, o, v, 0, 0)
#define TRACE_WRITE(a, o, v, n) tegra_trace_write(a, o, v, n, 1)
#define TRACE_IRQ_RAISE(a, i) do {tegra_trace_irq(a, i->n, 1); qemu_irq_raise(i);} while (0)
#define TRACE_IRQ_LOWER(a, i) do {tegra_trace_irq(a, i->n, 0); qemu_irq_lower(i);} while (0)
#define TRACE_IRQ_SET(a, i, v) do {tegra_trace_irq(a, (i)->n, v); qemu_set_irq(i,v);} while (0)
#define TPRINT(f, ...) tegra_trace_text_message(f, ## __VA_ARGS__)
#define TRACE_CDMA(d, g, c) tegra_trace_cdma(d, g, c)
#define TRACE_CDMA_START(c) tegra_trace_cdma(0xA << 28, 0, c)
#define TRACE_CDMA_STOP(c) tegra_trace_cdma(0xB << 28, 0, c)
#else
#define TRACE_READ(a, o, v) do {} while (0)
#define TRACE_WRITE(a, o, v, n) do {} while (0)
#define TRACE_IRQ_RAISE(a, i) do {qemu_irq_raise(i);} while (0)
#define TRACE_IRQ_LOWER(a, i) do {qemu_irq_lower(i);} while (0)
#define TRACE_IRQ_SET(a, i, v) do {qemu_set_irq(i,v);} while (0)
#define TPRINT(f, ...) do {} while (0)
#define TRACE_CDMA(d, g, c) do {} while (0)
#define TRACE_CDMA_START(c) do {} while (0)
#define TRACE_CDMA_STOP(c) do {} while (0)
#endif

void tegra_trace_irq(uint32_t hwaddr, uint32_t hwirq, uint32_t status);

void tegra_trace_write(uint32_t hwaddr, uint32_t offset,
                       uint32_t value, uint32_t new_value, uint32_t is_write);

void tegra_trace_cdma(uint32_t data, uint32_t is_gather, uint32_t ch_id);

void tegra_trace_init(void);

void tegra_trace_text_message(const char* format, ...);
