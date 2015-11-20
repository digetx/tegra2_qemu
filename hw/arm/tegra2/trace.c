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

#define LOCAL_SOCKET

#include <arpa/inet.h>
#include <assert.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

#include <sysemu/sysemu.h>
#include "qemu/thread.h"

#include "ahb/host1x/include/host1x_cdma.h"
#include "tegra_trace.h"

#define SOCKET_FILE     "/tmp/trace.sock"

/* FIXME */
static uint64_t reset_time;

#define HOST1X_CDMA	0x1010

#define PACKET_TRACE_RW 0x11111111
struct __attribute__((packed, aligned(1))) trace_pkt_rw {
    uint32_t magic;
    uint32_t hwaddr;
    uint32_t offset;
    uint32_t value;
    uint32_t new_value;
    uint32_t is_write;
    uint32_t time;
    uint32_t cpu_pc;
    uint32_t cpu_id;
};

#define PACKET_TRACE_IRQ 0x22222222
struct __attribute__((packed, aligned(1))) trace_pkt_irq {
    uint32_t magic;
    uint32_t hwaddr;
    uint32_t hwirq;
    uint32_t status;
    uint32_t time;
    uint32_t cpu_pc;
    uint32_t cpu_id;
};

#define PACKET_TRACE_TXT 0x33333333
struct __attribute__((packed, aligned(1))) trace_pkt_txt {
    uint32_t magic;
    uint32_t text_sz;
//     uint32_t time;
    char text[1]; // \0
};

#define PACKET_TRACE_CDMA 0x44444444
struct __attribute__((packed, aligned(1))) trace_pkt_cdma {
    uint32_t magic;
    uint32_t time;
    uint32_t data;
    uint32_t is_gather;
    uint32_t ch_id;
};

static int msgsock = -1;

static int trace_write_socket(void *data, int size)
{
    if (msgsock < 0)
        return 0;

    while (write(msgsock, data, size) < 0) {
        if (errno != EINTR) {
            perror("writing on stream socket");
            close(msgsock);
            msgsock = -1;
            break;
        }
    }

    return 1;
}

void tegra_trace_text_message(const char* format, ...)
{
#ifdef _GNU_SOURCE
    struct trace_pkt_txt *W;
    va_list args;
    char *txt;
    size_t sz;
    int ret;

    va_start(args, format);
    ret = vasprintf(&txt, format, args);
    va_end(args);

    if (ret < 1)
        return;

    sz = sizeof(*W) + strlen(txt);
    W = malloc(sz);
    W->magic = htonl(PACKET_TRACE_TXT);
//     W->time = qemu_clock_get_us(QEMU_CLOCK_VIRTUAL);
    W->text_sz = htonl(strlen(txt) + 1);
    memcpy(W->text, txt, strlen(txt) + 1);
    free(txt);

    if (!trace_write_socket(W, sz))
        printf("%s", W->text);

    free(W);
#endif
}

void tegra_trace_irq(uint32_t hwaddr, uint32_t hwirq, uint32_t status)
{
    uint32_t time = qemu_clock_get_us(QEMU_CLOCK_VIRTUAL);
    struct trace_pkt_irq W = {
        htonl(PACKET_TRACE_IRQ),
        htonl(hwaddr),
        htonl(hwirq),
        htonl(status),
        htonl(time - reset_time),
        htonl(0),
        htonl(0)
    };

    trace_write_socket(&W, sizeof(W));
}

void tegra_trace_write(uint32_t hwaddr, uint32_t offset,
                       uint32_t value, uint32_t new_value, uint32_t is_write)
{
    CPUState *cs = CPU(current_cpu);
    ARMCPU *cpu = ARM_CPU(cs);
    uint32_t cpu_pc = cpu ? cpu->env.regs[15] : 0;
    uint32_t time = qemu_clock_get_us(QEMU_CLOCK_VIRTUAL);
    uint32_t cpu_id = host1x_cdma_ptr ? HOST1X_CDMA : (cs ? cs->cpu_index : 0xFF);
    struct trace_pkt_rw W = {
        htonl(PACKET_TRACE_RW),
        htonl(hwaddr),
        htonl(offset),
        htonl(value),
        htonl(new_value),
        htonl(is_write),
        htonl(time - reset_time),
        htonl(cpu_pc),
        htonl(cpu_id)
    };

    trace_write_socket(&W, sizeof(W));
}

void tegra_trace_cdma(uint32_t data, uint32_t is_gather, uint32_t ch_id)
{
    uint32_t time = qemu_clock_get_us(QEMU_CLOCK_VIRTUAL);
    struct trace_pkt_cdma W = {
        htonl(PACKET_TRACE_CDMA),
        htonl(time),
        htonl(data),
        htonl(is_gather),
        htonl(ch_id)
    };

    trace_write_socket(&W, sizeof(W));
}

void tegra_trace_init(void)
{
#ifdef TEGRA_TRACE
    static int sock = -1;
    int on = 1;
#ifdef LOCAL_SOCKET
    struct sockaddr_un server;
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, SOCKET_FILE);

    unlink(SOCKET_FILE);
    if (sock != -1)
        close(sock);
    sock = socket(PF_UNIX, SOCK_STREAM, PF_UNIX);
#else
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(19191);
    if (sock != -1)
        close(sock);
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif // LOCAL_SOCKET
    if (sock < 0) {
        perror("opening stream socket");
        exit(-1);
    }

    if (msgsock != -1)
        close(msgsock);

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    if (bind(sock, (struct sockaddr *) &server, sizeof(server))) {
        perror("binding stream socket");
        exit(-1);
    }

    printf("Waiting for trace viewer connection...\n");
    listen(sock, 1);

    msgsock = accept(sock, NULL, NULL);
    assert(msgsock != -1);

    reset_time = qemu_clock_get_us(QEMU_CLOCK_VIRTUAL);
#endif // TEGRA_TRACE
}
