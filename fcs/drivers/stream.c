/*
Copyright (C) 2013 Ben Dyer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "stream.h"
#include "../stats/stats.h"
#include "../util/util.h"

#ifdef __TI_COMPILER_VERSION__
#include "../hardware/int-uart.h"
#include "../hardware/emif-uart.h"
#include "../hardware/ft232h.h"
#endif

/*
RX/TX buffers per serial link, with read/write indices -- these form circular
buffers.
*/
static uint8_t rx_buffers[FCS_STREAM_NUM_DEVICES][FCS_STREAM_BUFFER_SIZE];
static uint8_t tx_buffers[FCS_STREAM_NUM_DEVICES][FCS_STREAM_BUFFER_SIZE];
static uint16_t rx_read_idx[FCS_STREAM_NUM_DEVICES];
static uint16_t rx_write_idx[FCS_STREAM_NUM_DEVICES];
static uint16_t tx_read_idx[FCS_STREAM_NUM_DEVICES];
static uint16_t tx_write_idx[FCS_STREAM_NUM_DEVICES];

size_t _fcs_stream_write_to_rx_buffer(uint8_t buffer_idx,
const uint8_t *restrict val, size_t len);
size_t _fcs_stream_read_from_tx_buffer(uint8_t buffer_idx,
uint8_t *restrict val, size_t len);


/* Buffer access functions for the test harness */
size_t _fcs_stream_write_to_rx_buffer(uint8_t buffer_idx,
const uint8_t *restrict val, size_t len) {
    fcs_assert(buffer_idx < FCS_STREAM_NUM_DEVICES);
    fcs_assert(len < FCS_STREAM_BUFFER_SIZE);

    size_t i;
    #pragma MUST_ITERATE(1, FCS_STREAM_BUFFER_SIZE)
    for (i = 0; i < len; rx_write_idx[buffer_idx]++, i++) {
        rx_buffers[buffer_idx][
            rx_write_idx[buffer_idx] & FCS_STREAM_BUFFER_MASK] = val[i];
    }

    return i;
}

size_t _fcs_stream_read_from_tx_buffer(uint8_t buffer_idx,
uint8_t *restrict val, size_t len) {
    fcs_assert(buffer_idx < FCS_STREAM_NUM_DEVICES);
    fcs_assert(len <= FCS_STREAM_BUFFER_SIZE);

    size_t i;
    #pragma MUST_ITERATE(1, FCS_STREAM_BUFFER_SIZE)
    for (i = 0; i < len && tx_write_idx[buffer_idx] != tx_read_idx[buffer_idx];
            tx_read_idx[buffer_idx]++, i++) {
        val[i] = tx_buffers[buffer_idx][
            tx_read_idx[buffer_idx] & FCS_STREAM_BUFFER_MASK];
    }

    return i;
}

/*
fcs_stream_set_rate - if the stream device is a UART, sets the connection
baud rate to the value provided in "baud".

Returns FCS_STREAM_OK if OK, or FCS_STREAM_ERROR if the device is not a UART
or the baud rate is not valid.
*/
enum fcs_stream_result_t fcs_stream_set_rate(enum fcs_stream_device_t dev,
uint32_t baud) {
    fcs_assert(dev < FCS_STREAM_NUM_DEVICES);

    if (dev != FCS_STREAM_UART_INT0 && dev != FCS_STREAM_UART_INT1 &&
            dev != FCS_STREAM_UART_EXT0 && dev != FCS_STREAM_UART_EXT1 &&
            dev != FCS_STREAM_USB) {
        return FCS_STREAM_ERROR;
    } else if (baud < 57600 || baud > FCS_STREAM_MAX_RATE) {
        return FCS_STREAM_ERROR;
    }

#ifdef __TI_COMPILER_VERSION__
    if (dev == FCS_STREAM_UART_INT0 || dev == FCS_STREAM_UART_INT1) {
        /* Configure internal UART baud rate */
        uint8_t dev_idx = dev == FCS_STREAM_UART_INT0 ? 0 : 1;
        fcs_int_uart_set_baud_rate(dev_idx, baud);
        fcs_int_uart_reset(dev_idx);
    } else if (dev == FCS_STREAM_UART_EXT0 || dev == FCS_STREAM_UART_EXT1) {
        /* Configure external UART baud rate */
        uint8_t dev_idx = dev == FCS_STREAM_UART_EXT0 ? 0 : 1;
        fcs_emif_uart_set_baud_rate(dev_idx, baud);
        fcs_emif_uart_reset(dev_idx);
    } else if (dev == FCS_STREAM_USB) {
        /* Nothing to do here -- no rates need to be set. */
    } else {
        fcs_assert(false);
    }
#endif

    return FCS_STREAM_OK;
}

/*
fcs_stream_open -- configure a stream and reset its buffer state
*/
enum fcs_stream_result_t fcs_stream_open(enum fcs_stream_device_t dev) {
    fcs_assert(dev < FCS_STREAM_NUM_DEVICES);

    rx_read_idx[dev] = 0;
    rx_write_idx[dev] = 0;

    fcs_global_counters.stream_reset[dev]++;

#ifdef __TI_COMPILER_VERSION__
    if (dev == FCS_STREAM_UART_INT0 || dev == FCS_STREAM_UART_INT1) {
        /* Start internal UART RX EDMA transfer */
        uint8_t dev_idx = dev == FCS_STREAM_UART_INT0 ? 0 : 1;
        fcs_int_uart_start_rx_edma(
            dev_idx, rx_buffers[dev], FCS_STREAM_BUFFER_SIZE);
    } else if (dev == FCS_STREAM_UART_EXT0 || dev == FCS_STREAM_UART_EXT1) {
        /* Start external UART RX EDMA transfer */
        uint8_t dev_idx = dev == FCS_STREAM_UART_EXT0 ? 0 : 1;
        fcs_emif_uart_start_rx_edma(
            dev_idx, rx_buffers[dev], FCS_STREAM_BUFFER_SIZE);
    } else if (dev == FCS_STREAM_USB) {
        /* Set up the device */
        fcs_ft232h_reset();
    } else {
        fcs_assert(false);
    }
#endif

    return FCS_STREAM_OK;
}

/*
fcs_stream_check_error -- checks if an error has occurred
*/
enum fcs_stream_result_t fcs_stream_check_error(
enum fcs_stream_device_t dev) {
    fcs_assert(dev < FCS_STREAM_NUM_DEVICES);

    uint32_t err = 0;

#ifdef __TI_COMPILER_VERSION__
    if (dev == FCS_STREAM_UART_INT0 || dev == FCS_STREAM_UART_INT1) {
        err = fcs_int_uart_check_error(dev == FCS_STREAM_UART_INT0 ? 0 : 1);
    } else if (dev == FCS_STREAM_UART_EXT0 || dev == FCS_STREAM_UART_EXT1) {
        err = fcs_emif_uart_check_error(dev == FCS_STREAM_UART_EXT0 ? 0 : 1);
    } else if (dev == FCS_STREAM_USB) {
        /* TODO? */
    } else {
        fcs_assert(false);
    }
#endif

    if (err) {
        fcs_global_counters.stream_rx_err[dev]++;
    }

    return err ? FCS_STREAM_ERROR : FCS_STREAM_OK;
}

/*
fcs_stream_read - copy up to nbytes characters to the output buffer.
Does not consume the characters copied, so they will be returned by future
reads unless fcs_stream_consume is called.

Returns the number of bytes copied, in the range [0, nbytes].
*/
size_t fcs_stream_read(enum fcs_stream_device_t dev, uint8_t *restrict buf,
size_t nbytes) {
    fcs_assert(dev < FCS_STREAM_NUM_DEVICES);
    fcs_assert(nbytes < FCS_STREAM_BUFFER_SIZE);
    fcs_assert(buf);

#ifdef __TI_COMPILER_VERSION__
     uint8_t dev_idx;
    /* Update rx_write_idx based on current DMA state */
    if (dev == FCS_STREAM_UART_INT0 || dev == FCS_STREAM_UART_INT1) {
        dev_idx = dev == FCS_STREAM_UART_INT0 ? 0 : 1;
        rx_write_idx[dev] = fcs_int_uart_get_rx_edma_count(dev_idx);
    } else if (dev == FCS_STREAM_UART_EXT0 || dev == FCS_STREAM_UART_EXT1) {
        dev_idx = dev == FCS_STREAM_UART_EXT0 ? 0 : 1;
        rx_write_idx[dev] = fcs_emif_uart_get_rx_edma_count(dev_idx);
    } else if (dev == FCS_STREAM_USB) {
        rx_write_idx[dev] = 0;
    } else {
        fcs_assert(false);
    }
#endif

    size_t i;
    for (i = 0;
            i < nbytes &&
            ((rx_read_idx[dev] - rx_write_idx[dev]) & FCS_STREAM_BUFFER_MASK);
            rx_read_idx[dev]++, i++) {
        buf[i] = rx_buffers[dev][rx_read_idx[dev] & FCS_STREAM_BUFFER_MASK];
    }

    fcs_global_counters.stream_rx_byte[dev] += i;

    return i;
}

/*
fcs_stream_write - writes up to "nbytes" from "buf" into the device's output
buffer.

Returns the number of bytes actually written, in the range [0, nbytes].
Assuming the caller is writing at a rate lower than the maximum send rate of
the device, the return value will always be equal to "nbytes".
*/
size_t fcs_stream_write(enum fcs_stream_device_t dev,
const uint8_t *restrict buf, size_t nbytes) {
    fcs_assert(dev < FCS_STREAM_NUM_DEVICES);
    fcs_assert(nbytes && nbytes < FCS_STREAM_BUFFER_SIZE);
    fcs_assert(buf);

#ifdef __TI_COMPILER_VERSION__
    uint8_t dev_idx;
    /* Update rx_write_idx / tx_read_idx based on current DMA state */
    if (dev == FCS_STREAM_UART_INT0 || dev == FCS_STREAM_UART_INT1) {
        dev_idx = dev == FCS_STREAM_UART_INT0 ? 0 : 1;
        tx_read_idx[dev] = fcs_int_uart_get_tx_edma_count(dev_idx);
    } else if (dev == FCS_STREAM_UART_EXT0 || dev == FCS_STREAM_UART_EXT1) {
        dev_idx = dev == FCS_STREAM_UART_EXT0 ? 0 : 1;
        tx_read_idx[dev] = fcs_emif_uart_get_tx_edma_count(dev_idx);
    } else if (dev == FCS_STREAM_USB) {
        tx_read_idx[dev] = fcs_ft232h_get_tx_edma_count();
    } else {
        fcs_assert(false);
    }
#endif

    /*
    If the previous write hasn't finished, just return zero to indicate no
    bytes have been written for the current request.
    */
    if (tx_write_idx[dev] != tx_read_idx[dev]) {
        return 0;
    }

    memcpy(&tx_buffers[dev][0], buf, nbytes);
    tx_write_idx[dev] = (uint16_t)nbytes;
    tx_read_idx[dev] = 0;

    fcs_global_counters.stream_tx_byte[dev] += nbytes;

#ifdef __TI_COMPILER_VERSION__
    /* Trigger a DMA transfer / copy the number of bytes to the PaRAM */
    if (dev == FCS_STREAM_UART_INT0 || dev == FCS_STREAM_UART_INT1) {
        uint8_t dev_idx = dev == FCS_STREAM_UART_INT0 ? 0 : 1;
        fcs_int_uart_start_tx_edma(dev_idx, &tx_buffers[dev][0], nbytes);
    } else if (dev == FCS_STREAM_UART_EXT0 || dev == FCS_STREAM_UART_EXT1) {
        uint8_t dev_idx = dev == FCS_STREAM_UART_EXT0 ? 0 : 1;
        fcs_emif_uart_start_tx_edma(dev_idx, &tx_buffers[dev][0], nbytes);
    } else if (dev == FCS_STREAM_USB) {
        fcs_ft232h_start_tx_edma(&tx_buffers[dev][0], nbytes);
    } else {
        fcs_assert(false);
    }
#endif

    return nbytes;
}
