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

#ifndef TEGRA_HOST1X_CMD_PROCESSOR_H
#define TEGRA_HOST1X_CMD_PROCESSOR_H

#define SETCL   0
#define INCR    1
#define NONINCR 2
#define MASK    3
#define IMM     4
#define RESTART 5
#define GATHER  6
#define EXTEND  0xE
#define CHDONE  0xF

#define ACQUIRE_MLOCK   0
#define RELEASE_MLOCK   1

#define CMD_OPCODE(data)    (data >> 28)

typedef union setcl_op {
    struct {
        unsigned int mask:6;
        unsigned int class_id:10;
        unsigned int offset:12;
        unsigned int opcode:4;
    };

    uint32_t reg32;
} setcl_op;

typedef union incr_op {
    struct {
        unsigned int count:16;
        unsigned int offset:12;
        unsigned int opcode:4;
    };

    uint32_t reg32;
} incr_op;

#define nonincr_op incr_op

typedef union mask_op {
    struct {
        unsigned int mask:16;
        unsigned int offset:12;
        unsigned int opcode:4;
    };

    uint32_t reg32;
} mask_op;

typedef union imm_op {
    struct {
        unsigned int immdata:16;
        unsigned int offset:12;
        unsigned int opcode:4;
    };

    uint32_t reg32;
} imm_op;

typedef union gather_op {
    struct {
        unsigned int count:14;
        unsigned int incr:1;
        unsigned int insert:1;
        unsigned int offset:12;
        unsigned int opcode:4;
    };

    uint32_t reg32;
} gather_op;

typedef union extend_op {
    struct {
        unsigned int value:24;
        unsigned int subop:4;
        unsigned int opcode:4;
    };

    uint32_t reg32;
} extend_op;

typedef union restart_op {
    struct {
        unsigned int offset:28;
        unsigned int opcode:4;
    };

    uint32_t reg32;
} restart_op;

void process_cmd_buf(struct host1x_dma_gather *gather);

#endif // TEGRA_HOST1X_CMD_PROCESSOR_H
