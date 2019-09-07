/*
 * NRF24L01+ SPI Slave
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

#include "hw/ssi.h"

#ifndef NRF24L01PLUS_ERR_DEBUG
#define NRF24L01PLUS_ERR_DEBUG 0
#endif

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (NRF24L01PLUS_ERR_DEBUG >= lvl) { \
        qemu_log("%s: " fmt, __func__, ## args); \
    } \
} while (0);

#define DB_PRINT(fmt, args...) DB_PRINT_L(1, fmt, ## args)

#define R_REGISTER 0xE0
#define W_REGISTER 0xC0
#define R_RX_PAYLOAD 0x9E
#define R_TX_PAYLOAD 0x5F
#define FLUSH_TX 0x1E
#define FLUSH_RX 0x1D
#define NOP 0xFF

#define CONFIG_REG 0x00
#define STATUS_REG 0x07

#define STATUS_REG_RX_DR 6

typedef enum {
    NRF24L01P_CMD,
    NRF24L01P_WRITE,
    NRF24L01P_READ,
    NRF24L01P_READ_RX_PAYLOAD,
    NRF24L01P_WRITE_TX_PAYLOAD,
    NRF24L01P_FLUSH_TX,
    NRF24L01P_FLUSH_RX,
} nrf24l01plus_mode;

typedef struct {
    SSISlave ssidev;
    nrf24l01plus_mode mode;
    uint32_t register_map;

    uint32_t status_reg;
    uint32_t config_reg;
} nrf24l01plus_state;

static uint32_t nrf24l01plus_transfer(SSISlave *dev, uint32_t val)
{
    nrf24l01plus_state *s = FROM_SSI_SLAVE(nrf24l01plus_state, dev);

    DB_PRINT("Mode is: %d; Value is: 0x%x\n", s->mode, val);

    if (val == NOP) {
        return 0;
    }

    if (s->mode == NRF24L01P_CMD) {
        if (!(val & R_REGISTER)) {
            s->mode = NRF24L01P_READ;
            s->register_map = 0b11111 & val;
        } else if (!(val & W_REGISTER)) {
            s->mode = NRF24L01P_WRITE;
            s->register_map = 0b11111 & val;
        } else if (val == R_RX_PAYLOAD) {
            s->mode = NRF24L01P_READ_RX_PAYLOAD;
        } else if (val == R_TX_PAYLOAD) {
            s->mode = NRF24L01P_WRITE_TX_PAYLOAD;
        } else if (val == FLUSH_TX) {
            s->mode = NRF24L01P_FLUSH_TX;
        } else if (val == FLUSH_RX) {
            s->mode = NRF24L01P_FLUSH_RX;
        } else {
            qemu_log_mask(LOG_UNIMP,
                          "nRF24L01+_Transfer: Bad command or unimplemented " \
                          "0x%x\n", val);
        }
    }

    DB_PRINT("New mode is: %d\n", s->mode);

    switch (s->mode) {
    case NRF24L01P_WRITE:
        DB_PRINT("Writing Register: 0x%x\n", s->register_map);
        s->mode = NRF24L01P_CMD;

        switch(s->register_map) {
        case STATUS_REG:
            s->status_reg &= ~val;
            break;
        default:
            qemu_log_mask(LOG_UNIMP,
                          "nRF24L01+_Read: Bad address or unimplemented " \
                          "0x%x\n", s->register_map);
        }
        break;
    case NRF24L01P_READ:
        DB_PRINT("Reading Register: 0x%x\n", s->register_map);
        s->mode = NRF24L01P_CMD;

        switch(s->register_map) {
        case CONFIG_REG:
            return s->config_reg;
        case STATUS_REG:
            /* Let's fake that data is avalible */
            s->status_reg |= (1 << STATUS_REG_RX_DR);
            return s->status_reg;
        default:
            qemu_log_mask(LOG_UNIMP,
                          "nRF24L01+_Read: Bad address or unimplemented " \
                          "0x%x\n", s->register_map);
        }
        break;
    case NRF24L01P_READ_RX_PAYLOAD:
        // Read from internal FIFO
        s->mode = NRF24L01P_CMD;
        return 0xFF;
    case NRF24L01P_WRITE_TX_PAYLOAD:
        //Write TX payload
        s->mode = NRF24L01P_CMD;
        break;
    case NRF24L01P_FLUSH_TX:
        /* Flush the TX FIFO */
        s->mode = NRF24L01P_CMD;
        break;
    case NRF24L01P_FLUSH_RX:
        /* Flush the RX FIFO */
        s->mode = NRF24L01P_CMD;
        return 0xFF;
    default:
        return 1;
    }
    return 0;
}

static int nrf24l01plus_init(SSISlave *d)
{
    nrf24l01plus_state *s = FROM_SSI_SLAVE(nrf24l01plus_state, d);

    s->mode = NRF24L01P_CMD;

    return 0;
}

static void nrf24l01plus_class_init(ObjectClass *klass, void *data)
{
    SSISlaveClass *k = SSI_SLAVE_CLASS(klass);

    k->init = nrf24l01plus_init;
    k->transfer = nrf24l01plus_transfer;
    k->cs_polarity = SSI_CS_LOW;
}

static const TypeInfo nrf24l01plus_info = {
    .name          = "nrf24l01plus",
    .parent        = TYPE_SSI_SLAVE,
    .instance_size = sizeof(nrf24l01plus_state),
    .class_init    = nrf24l01plus_class_init,
};

static void nrf24l01plus_register_types(void)
{
    type_register_static(&nrf24l01plus_info);
}

type_init(nrf24l01plus_register_types)
