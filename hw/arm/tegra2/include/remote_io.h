/*
 * ARM NVIDIA Tegra2 emulation.
 *
 * Copyright (c) 2015 Dmitry Osipenko <digetx@gmail.com>
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

uint32_t remote_io_read(uint32_t addr, int size);
void remote_io_write(uint32_t value, uint32_t addr, int size);
void remote_io_watch_irq(uint32_t base_addr, qemu_irq *irq);
void remote_io_rst_set(uint8_t id, int enb);
void remote_io_clk_set(uint8_t id, int enb);
void remote_io_init(const char *addr);
void remote_io_read_mem_range(uint8_t *data, uint32_t addr, uint32_t size);
void remote_io_read_cache_invalidate(void);
