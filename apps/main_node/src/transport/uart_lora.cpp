#include "uart_lora.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/crc.h>
#include <deos/deos.h>
#include <deos/deos_fault.h>

LOG_MODULE_REGISTER(uart_lora, LOG_LEVEL_INF);

#define LORA_TX_QUEUE_SIZE 16
#define LORA_THREAD_STACK_SIZE 1024
#define LORA_THREAD_PRIO 5
#define RING_BUF_SIZE 1024

/* Framing Constants */
#define LORA_MAGIC_1       0xAA
#define LORA_MAGIC_2       0x55
#define LORA_FLAG_NONE     0x00
#define LORA_FLAG_ACK_REQ  0x01
#define LORA_FLAG_IS_ACK   0x02
#define LORA_MAX_PAYLOAD   100

static K_KERNEL_STACK_DEFINE(lora_rx_stack, LORA_THREAD_STACK_SIZE);
static K_KERNEL_STACK_DEFINE(lora_tx_stack, LORA_THREAD_STACK_SIZE);
static struct k_thread lora_rx_thread_data;
static struct k_thread lora_tx_thread_data;

UartLora& UartLora::getInstance() {
    static UartLora instance;
    return instance;
}

UartLora::UartLora() : m_uart_dev(DEVICE_DT_GET(DT_ALIAS(lora_uart))) {
    ring_buf_init(&m_rx_rb, RING_BUF_SIZE, m_rx_rb_buffer);
    k_msgq_init(&m_tx_q, m_tx_q_buffer, sizeof(deos_message_t), LORA_TX_QUEUE_SIZE);
}

int UartLora::tx_callback(const deos_message_t *msg) {
    return getInstance().send(msg);
}

int UartLora::send(const deos_message_t *msg) {
    if (k_msgq_put(&m_tx_q, msg, K_NO_WAIT) != 0) {
        LOG_WRN("LoRa TX queue full, dropping message");
        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        return -ENOSPC;
    }
    return 0;
}

void UartLora::uart_rx_cb(const struct device *dev, void *user_data) {
    UartLora* instance = static_cast<UartLora*>(user_data);
    uint8_t c;

    uart_irq_update(dev);

    if (uart_irq_rx_ready(dev)) {
        while (uart_fifo_read(dev, &c, 1) == 1) {
            if (ring_buf_put(&instance->m_rx_rb, &c, 1) == 0) {
                // Overflow
            }
        }
    }
}

void UartLora::send_lora_frame(uint8_t flag, const uint8_t *payload, uint8_t len) {
    uint8_t header[4] = { LORA_MAGIC_1, LORA_MAGIC_2, flag, len };
    uint16_t crc = crc16_ccitt(0xFFFF, &flag, 1);
    crc = crc16_ccitt(crc, &len, 1);
    if (len > 0) crc = crc16_ccitt(crc, payload, len);
    
    for(int i=0; i<4; i++) uart_poll_out(m_uart_dev, header[i]);
    for(int i=0; i<len; i++) uart_poll_out(m_uart_dev, payload[i]);
    uart_poll_out(m_uart_dev, crc & 0xFF);
    uart_poll_out(m_uart_dev, (crc >> 8) & 0xFF);
}

void UartLora::send_deos_lora(uint8_t flag, const deos_message_t *msg) {
    size_t header_sz = offsetof(deos_message_t, payload);
    uint8_t buf[LORA_MAX_PAYLOAD];
    
    memcpy(buf, msg, header_sz);
    if (msg->payload_len > 0 && msg->payload_len <= DEOS_MAX_PAYLOAD_LEN) {
        memcpy(buf + header_sz, msg->payload, msg->payload_len);
    }
    
    uint8_t len = header_sz + msg->payload_len;
    send_lora_frame(flag, buf, len);
}

void UartLora::tx_thread_entry(void *p1, void *p2, void *p3) {
    auto instance = static_cast<UartLora*>(p1);
    instance->tx_thread();
}

void UartLora::tx_thread() {
    deos_message_t msg;
    LOG_INF("LoRa TX Thread started with Dynamic Framing (OOP)");

    while (1) {
        if (k_msgq_get(&m_tx_q, &msg, K_FOREVER) == 0) {
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

void UartLora::rx_thread_entry(void *p1, void *p2, void *p3) {
    auto instance = static_cast<UartLora*>(p1);
    instance->rx_thread();
}

void UartLora::rx_thread() {
    LOG_INF("LoRa RX Thread started with Framing and ACK Support! (OOP)");
    
    rx_state_t rx_state = STATE_WAIT_MAGIC1;
    uint8_t rx_len = 0, rx_flag = 0;
    uint8_t rx_buf[LORA_MAX_PAYLOAD];
    int rx_idx = 0;
    uint16_t rx_crc = 0;

    while (1) {
        uint8_t c;
        if (ring_buf_get(&m_rx_rb, &c, 1) == 1) {
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
                        if (rx_flag == LORA_FLAG_ACK_REQ) {
                            uint8_t ack_payload[2] = { static_cast<uint8_t>(calc_crc & 0xFF), static_cast<uint8_t>((calc_crc >> 8) & 0xFF) };
                            send_lora_frame(LORA_FLAG_IS_ACK, ack_payload, 2);
                            LOG_INF("uart_lora: Sent ACK for valid packet (CRC 0x%04X)", calc_crc);
                        }
                        
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
            k_msleep(1);
        }
    }
}

int UartLora::init() {
    const struct device *gpiog = DEVICE_DT_GET(DT_NODELABEL(gpiog));
    const struct device *gpioe = DEVICE_DT_GET(DT_NODELABEL(gpioe));

    if (!device_is_ready(m_uart_dev)) {
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

    uart_irq_callback_user_data_set(m_uart_dev, uart_rx_cb, this);
    uart_irq_rx_enable(m_uart_dev);

    deos_router_register_transport(DEOS_TRANSPORT_UART_LORA, tx_callback);

    k_thread_create(&lora_tx_thread_data, lora_tx_stack,
                    K_KERNEL_STACK_SIZEOF(lora_tx_stack),
                    tx_thread_entry, this, NULL, NULL,
                    LORA_THREAD_PRIO, 0, K_NO_WAIT);

    k_thread_create(&lora_rx_thread_data, lora_rx_stack,
                    K_KERNEL_STACK_SIZEOF(lora_rx_stack),
                    rx_thread_entry, this, NULL, NULL,
                    LORA_THREAD_PRIO, 0, K_NO_WAIT);

    LOG_INF("LoRa UART interface initialized");
    return 0;
}
