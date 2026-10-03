#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/ring_buffer.h>
#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "uart_lora.h"

LOG_MODULE_REGISTER(uart_lora, LOG_LEVEL_INF);

#define LORA_TX_QUEUE_SIZE 16
#define LORA_THREAD_STACK_SIZE 1024
#define LORA_THREAD_PRIO 5

#define RING_BUF_SIZE 1024
RING_BUF_DECLARE(lora_rx_rb, RING_BUF_SIZE);

/* Threads and queues */
K_MSGQ_DEFINE(lora_tx_q, sizeof(deos_message_t), LORA_TX_QUEUE_SIZE, 4);

static struct k_thread lora_rx_thread_data;
static K_KERNEL_STACK_DEFINE(lora_rx_stack, LORA_THREAD_STACK_SIZE);

static struct k_thread lora_tx_thread_data;
static K_KERNEL_STACK_DEFINE(lora_tx_stack, LORA_THREAD_STACK_SIZE);

static const struct device *uart_dev = DEVICE_DT_GET(DT_ALIAS(lora_uart));

/* UART RX Interrupt Callback */
static void uart_rx_cb(const struct device *dev, void *user_data)
{
    uint8_t c;

    uart_irq_update(dev);

    if (uart_irq_rx_ready(dev)) {
        while (uart_fifo_read(dev, &c, 1) == 1) {
            if (ring_buf_put(&lora_rx_rb, &c, 1) == 0) {
                // Overflow (We can't safely raise DEOS fault in ISR)
            }
        }
    }
}

/* Router TX callback */
static int lora_tx_callback(const deos_message_t *msg)
{
    if (k_msgq_put(&lora_tx_q, msg, K_NO_WAIT) != 0) {
        LOG_WRN("LoRa TX queue full, dropping message");
        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        return -ENOSPC;
    }
    return 0;
}

/* Framing Constants */
#define LORA_MAGIC_1       0xAA
#define LORA_MAGIC_2       0x55
#define LORA_FLAG_NONE     0x00
#define LORA_FLAG_ACK_REQ  0x01
#define LORA_FLAG_IS_ACK   0x02
#define LORA_MAX_PAYLOAD   100

#include <zephyr/sys/crc.h>
#include <stddef.h>

/* Helper to send a frame (blocks in thread context) */
static void send_lora_frame(uint8_t flag, const uint8_t *payload, uint8_t len)
{
    uint8_t header[4] = { LORA_MAGIC_1, LORA_MAGIC_2, flag, len };
    uint16_t crc = crc16_ccitt(0xFFFF, &flag, 1);
    crc = crc16_ccitt(crc, &len, 1);
    if (len > 0) crc = crc16_ccitt(crc, payload, len);
    
    for(int i=0; i<4; i++) uart_poll_out(uart_dev, header[i]);
    for(int i=0; i<len; i++) uart_poll_out(uart_dev, payload[i]);
    uart_poll_out(uart_dev, crc & 0xFF);
    uart_poll_out(uart_dev, (crc >> 8) & 0xFF);
}

/* Helper to serialize and send DEOS message dynamically */
static void send_deos_lora(uint8_t flag, const deos_message_t *msg)
{
    size_t header_sz = offsetof(deos_message_t, payload);
    uint8_t buf[LORA_MAX_PAYLOAD];
    
    memcpy(buf, msg, header_sz);
    if (msg->payload_len > 0 && msg->payload_len <= DEOS_MAX_PAYLOAD_LEN) {
        memcpy(buf + header_sz, msg->payload, msg->payload_len);
    }
    
    uint8_t len = header_sz + msg->payload_len;
    send_lora_frame(flag, buf, len);
}

/* TX Thread (Sends data out over UART) */
static void lora_tx_thread(void *p1, void *p2, void *p3)
{
    deos_message_t msg;
    LOG_INF("LoRa TX Thread started with Dynamic Framing");

    while (1) {
        if (k_msgq_get(&lora_tx_q, &msg, K_FOREVER) == 0) {
            send_deos_lora(LORA_FLAG_NONE, &msg);
            LOG_DBG("LoRa TX: Sent framed message (Cmd: 0x%02X, Len: %d)", msg.command, msg.payload_len);
        }
    }
}

typedef enum {
    STATE_WAIT_MAGIC1, STATE_WAIT_MAGIC2,
    STATE_WAIT_FLAG, STATE_WAIT_LENGTH,
    STATE_WAIT_PAYLOAD, STATE_WAIT_CRC1, STATE_WAIT_CRC2
} rx_state_t;

/* RX Thread (Receives data from UART Ring Buffer) */
static void lora_rx_thread(void *p1, void *p2, void *p3)
{
    LOG_INF("LoRa RX Thread started with Framing and ACK Support!");
    
    rx_state_t rx_state = STATE_WAIT_MAGIC1;
    uint8_t rx_len = 0, rx_flag = 0;
    uint8_t rx_buf[LORA_MAX_PAYLOAD];
    int rx_idx = 0;
    uint16_t rx_crc = 0;

    while (1) {
        uint8_t c;
        if (ring_buf_get(&lora_rx_rb, &c, 1) == 1) {
            switch(rx_state) {
                case STATE_WAIT_MAGIC1:
                    if (c == LORA_MAGIC_1) rx_state = STATE_WAIT_MAGIC2;
                    break;
                case STATE_WAIT_MAGIC2:
                    if (c == LORA_MAGIC_2) rx_state = STATE_WAIT_FLAG;
                    else if (c != LORA_MAGIC_1) rx_state = STATE_WAIT_MAGIC1;
                    break;
                case STATE_WAIT_FLAG:
                    rx_flag = c;
                    rx_state = STATE_WAIT_LENGTH;
                    break;
                case STATE_WAIT_LENGTH:
                    rx_len = c;
                    rx_idx = 0;
                    if (rx_len > LORA_MAX_PAYLOAD) {
                        rx_state = STATE_WAIT_MAGIC1;
                    } else if (rx_len == 0) {
                        rx_state = STATE_WAIT_CRC1;
                    } else {
                        rx_state = STATE_WAIT_PAYLOAD;
                    }
                    break;
                case STATE_WAIT_PAYLOAD:
                    rx_buf[rx_idx++] = c;
                    if (rx_idx >= rx_len) rx_state = STATE_WAIT_CRC1;
                    break;
                case STATE_WAIT_CRC1:
                    rx_crc = c;
                    rx_state = STATE_WAIT_CRC2;
                    break;
                case STATE_WAIT_CRC2:
                    rx_crc |= (uint16_t)c << 8;
                    
                    uint16_t calc_crc = crc16_ccitt(0xFFFF, &rx_flag, 1);
                    calc_crc = crc16_ccitt(calc_crc, &rx_len, 1);
                    if (rx_len > 0) calc_crc = crc16_ccitt(calc_crc, rx_buf, rx_len);
                    
                    if (calc_crc == rx_crc) {
                        /* 1. Should we send ACK? */
                        if (rx_flag == LORA_FLAG_ACK_REQ) {
                            uint8_t ack_payload[2] = { calc_crc & 0xFF, (calc_crc >> 8) & 0xFF };
                            send_lora_frame(LORA_FLAG_IS_ACK, ack_payload, 2);
                            LOG_INF("uart_lora: Sent ACK for valid packet (CRC 0x%04X)", calc_crc);
                        }
                        
                        /* 2. Process data if it's not an ACK frame */
                        if (rx_flag != LORA_FLAG_IS_ACK) {
                            size_t header_sz = offsetof(deos_message_t, payload);
                            if (rx_len >= header_sz) {
                                deos_message_t msg;
                                memset(&msg, 0, sizeof(msg));
                                memcpy(&msg, rx_buf, header_sz);
                                msg.payload_len = rx_len - header_sz;
                                
                                if (msg.payload_len > 0 && msg.payload_len <= DEOS_MAX_PAYLOAD_LEN) {
                                    memcpy(msg.payload, rx_buf + header_sz, msg.payload_len);
                                }
                                
                                LOG_INF("uart_lora: RX SUCCESS (Dynamic, Len: %d)! Pushing Cmd: 0x%02X to Router...", msg.payload_len, msg.command);
                                deos_feed_message(&msg, DEOS_TRANSPORT_UART_LORA);
                            } else {
                                LOG_WRN("uart_lora: Received packet too small for DEOS Header!");
                            }
                        }
                    } else {
                        LOG_WRN("uart_lora: CRC mismatch! Calc: 0x%04X, Rx: 0x%04X", calc_crc, rx_crc);
                    }
                    rx_state = STATE_WAIT_MAGIC1;
                    break;
            }
        } else {
            k_msleep(1); /* Yield if ring buffer is empty */
        }
    }
}

int uart_lora_init(void)
{
    const struct device *gpiog = DEVICE_DT_GET(DT_NODELABEL(gpiog));
    const struct device *gpioe = DEVICE_DT_GET(DT_NODELABEL(gpioe));

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("LoRa UART device not ready!");
        return -ENODEV;
    }

    if (device_is_ready(gpiog)) {
        gpio_pin_configure(gpiog, 14, GPIO_OUTPUT_INACTIVE); /* M0 (D2) -> LOW */
    } else {
        LOG_WRN("gpiog for M0 not ready");
    }

    if (device_is_ready(gpioe)) {
        gpio_pin_configure(gpioe, 13, GPIO_OUTPUT_INACTIVE); /* M1 (D3) -> LOW */
    } else {
        LOG_WRN("gpioe for M1 not ready");
    }

    /* Configure UART interrupt */
    uart_irq_callback_user_data_set(uart_dev, uart_rx_cb, NULL);
    uart_irq_rx_enable(uart_dev);

    /* Register interface with the DEOS Router */
    deos_router_register_transport(DEOS_TRANSPORT_UART_LORA, lora_tx_callback);

    /* Start the TX and RX threads */
    k_thread_create(&lora_tx_thread_data, lora_tx_stack,
                    K_KERNEL_STACK_SIZEOF(lora_tx_stack),
                    lora_tx_thread, NULL, NULL, NULL,
                    LORA_THREAD_PRIO, 0, K_NO_WAIT);

    k_thread_create(&lora_rx_thread_data, lora_rx_stack,
                    K_KERNEL_STACK_SIZEOF(lora_rx_stack),
                    lora_rx_thread, NULL, NULL, NULL,
                    LORA_THREAD_PRIO, 0, K_NO_WAIT);

    LOG_INF("LoRa UART interface initialized");
    return 0;
}
