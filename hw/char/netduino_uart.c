/*
 * Netduino Plus 2 UART
 *
 * Copyright (c) 2014 Alistair Francis <alistair@alistair23.me>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "hw/sysbus.h"
#include "sysemu/char.h"
#include "hw/hw.h"
#include "net/net.h"

//#define DEBUG_NETUART

#ifdef DEBUG_NETUART
#define DPRINTF(fmt, ...) \
do { printf("netduino_uart: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#endif

#define TYPE_NETDUINO_UART "netduino_uart"
#define NETDUINO_UART(obj) \
    OBJECT_CHECK(struct net_uart, (obj), TYPE_NETDUINO_UART)

#define RX_FIFO_SIZE           16

struct net_uart {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    uint8_t rx_fifo[RX_FIFO_SIZE];

    NICState *nic;
    CharDriverState *chr;
    qemu_irq irq;
    NICConf conf;
};

static void uart_write_rx_fifo(void *opaque, const uint8_t *buf, int size)
{
    struct net_uart *s = opaque;
    int i;

    DPRINTF("RX_FIFO\n");

    for (i = 0; i < size; i++) {
        DPRINTF("Buf: %c", buf[i]);
        s->rx_fifo[i] = buf[i];
    }
    DPRINTF("\n");
}

static void uart_receive(void *opaque, const uint8_t *buf, int size)
{
    //struct net_uart *s = opaque;

    uart_write_rx_fifo(opaque, buf, size);
}

static void uart_event(void *opaque, int event)
{
    //struct net_uart *s = opaque;
    uint8_t buf = '\0';

    if (event == CHR_EVENT_BREAK) {
        uart_write_rx_fifo(opaque, &buf, 1);
    }
}

static uint64_t netduino_uart_read(void *opaque, hwaddr addr, unsigned int size)
{
    struct net_uart *s = opaque;

    DPRINTF("Read 0x%x\n", (uint) addr);

    switch (addr) {
        case 0x0:
            if (s && s->chr) {
                qemu_chr_accept_input(s->chr);
            } else {
                DPRINTF("NULL\n");
            }
            return 0xFFFFFFFF;
        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                          "net_uart_read: Bad offset %x\n", (int)addr);
            return 0;
    }

    return 0;
}

static void netduino_uart_write(void *opaque, hwaddr addr,
                       uint64_t val64, unsigned int size)
{
    struct net_uart *s = opaque;
    uint32_t value = (uint32_t) val64;
    unsigned char ch;

    DPRINTF("Write 0x%x, 0x%x\n", value, (uint) addr);

    switch (addr) {
        case 0x0:
            return;
        case 0x4:
            ch = value;
            if (s && s->chr) {
                qemu_chr_fe_write(s->chr, &ch, 1);
            } else {
                DPRINTF("NULL\n");
            }
            return;
        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                          "net_uart_write: Bad offset %x\n", (int)addr);
    }
}

static const MemoryRegionOps netduino_uart_ops = {
    .read = netduino_uart_read,
    .write = netduino_uart_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int netduino_uart_init(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    struct net_uart *s = NETDUINO_UART(dev);

    sysbus_init_irq(sbd, &s->irq);

    memory_region_init_io(&s->mmio, OBJECT(s), &netduino_uart_ops, s,
                          TYPE_NETDUINO_UART, 0x1000);
    sysbus_init_mmio(sbd, &s->mmio);

    s->chr = qemu_char_get_next_serial();

    if (s->chr) {
        qemu_chr_add_handlers(s->chr, NULL, uart_receive,
                              uart_event, s);
    }

    return 0;
}

static void netduino_uart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = netduino_uart_init;
    dc->props = NULL;
}

static const TypeInfo netduino_uart_info = {
    .name          = TYPE_NETDUINO_UART,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(struct net_uart),
    .class_init    = netduino_uart_class_init,
};

static void netduino_uart_register_types(void)
{
    type_register_static(&netduino_uart_info);
}

type_init(netduino_uart_register_types)
