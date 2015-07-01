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

#include "qemu/aes.h"

enum {
    XOR_DISABLED = 0,
    XOR_TOP = 2,
    XOR_BOTTOM,
    AES_OUT = 2,
    AES_IN
};

typedef struct tegra_aes_engine {
    uint8_t buffer[AES_BLOCK_SIZE];
    dma_addr_t dma_src_addr;
    dma_addr_t dma_dst_addr;
    AES_KEY key;
    int xor_pos;
    int iv_en;
    int is_enc;
    int rng_en;
} tegra_aes_engine;

void start_aes_engine(tegra_aes_engine *e, int blk_cnt);