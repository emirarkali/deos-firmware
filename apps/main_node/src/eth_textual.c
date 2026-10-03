#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "eth_textual.h"

LOG_MODULE_REGISTER(eth_textual, LOG_LEVEL_INF);

#define TEXTUAL_TX_QUEUE_SIZE 16
#define TEXTUAL_THREAD_STACK_SIZE 2048
#define TEXTUAL_THREAD_PRIO 5
#define TEXTUAL_TCP_PORT 5000

/* TX Queue to prevent blocking the router */
K_MSGQ_DEFINE(textual_tx_q, sizeof(deos_message_t), TEXTUAL_TX_QUEUE_SIZE, 4);

/* Threads */
static struct k_thread textual_server_thread_data;
static K_KERNEL_STACK_DEFINE(textual_server_stack, TEXTUAL_THREAD_STACK_SIZE);

static struct k_thread textual_tx_thread_data;
static K_KERNEL_STACK_DEFINE(textual_tx_stack, TEXTUAL_THREAD_STACK_SIZE);

/* Global client socket to allow TX thread to send data */
static int active_client_sock = -1;

/* Router TX callback */
static int textual_tx_callback(const deos_message_t *msg)
{
    /* If no client connected, just drop silently or log debug */
    if (active_client_sock < 0) {
        return -ENOTCONN;
    }

    /* Push to queue, do not block the router */
    if (k_msgq_put(&textual_tx_q, msg, K_NO_WAIT) != 0) {
        LOG_WRN("Textual TX queue full, dropping message");
        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        return -ENOSPC;
    }
    return 0;
}

/* TX Thread (Sends data out over TCP) */
static void textual_tx_thread(void *p1, void *p2, void *p3)
{
    deos_message_t msg;
    LOG_INF("Textual TCP TX Thread started");

    while (1) {
        if (k_msgq_get(&textual_tx_q, &msg, K_FOREVER) == 0) {
            if (active_client_sock >= 0) {
                /* Serialize to dynamic frame size */
                size_t header_sz = offsetof(deos_message_t, payload);
                uint8_t len = header_sz + msg.payload_len;
                
                uint8_t tx_buf[DEOS_MAX_PAYLOAD_LEN + 30];
                tx_buf[0] = len; /* 1 Byte Length */
                memcpy(&tx_buf[1], &msg, header_sz);
                if (msg.payload_len > 0) {
                    memcpy(&tx_buf[1 + header_sz], msg.payload, msg.payload_len);
                }

                /* Send over TCP */
                int sent = send(active_client_sock, tx_buf, len + 1, 0);
                if (sent < 0) {
                    LOG_ERR("TCP Send failed (%d), dropping client", errno);
                    close(active_client_sock);
                    active_client_sock = -1;
                }
            }
        }
    }
}

/* Server Thread (Listens for TCP and Receives data) */
static void textual_server_thread(void *p1, void *p2, void *p3)
{
    int serv_sock;
    struct sockaddr_in bind_addr;

    serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serv_sock < 0) {
        LOG_ERR("Failed to create TCP socket: %d", errno);
        return;
    }

    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(TEXTUAL_TCP_PORT);

    if (bind(serv_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        LOG_ERR("Failed to bind TCP socket: %d", errno);
        close(serv_sock);
        return;
    }

    if (listen(serv_sock, 1) < 0) {
        LOG_ERR("Failed to listen on TCP socket: %d", errno);
        close(serv_sock);
        return;
    }

    LOG_INF("Textual TCP Server listening on port %d", TEXTUAL_TCP_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        LOG_INF("Waiting for Textual UI connection...");
        int client = accept(serv_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        
        if (client < 0) {
            LOG_ERR("Accept failed: %d", errno);
            k_msleep(1000);
            continue;
        }

        LOG_INF("Textual UI Connected!");
        active_client_sock = client;

        while (1) {
            uint8_t rx_len;
            int ret = recv(client, &rx_len, 1, 0);
            if (ret <= 0) {
                LOG_INF("Textual UI Disconnected.");
                break;
            }

            /* Prevent buffer overflows */
            if (rx_len == 0 || rx_len > sizeof(deos_message_t)) {
                LOG_WRN("Invalid frame length %d received, closing connection.", rx_len);
                break;
            }

            uint8_t frame_buf[sizeof(deos_message_t)];
            int received = 0;
            
            /* Read exactly rx_len bytes for the frame */
            while (received < rx_len) {
                ret = recv(client, frame_buf + received, rx_len - received, 0);
                if (ret <= 0) break;
                received += ret;
            }

            if (received != rx_len) {
                LOG_WRN("Incomplete frame received.");
                break;
            }

            /* Successfully received full DEOS dynamic frame */
            size_t header_sz = offsetof(deos_message_t, payload);
            if (rx_len >= header_sz) {
                deos_message_t msg;
                memset(&msg, 0, sizeof(msg));
                memcpy(&msg, frame_buf, header_sz);
                msg.payload_len = rx_len - header_sz;
                
                if (msg.payload_len > 0) {
                    memcpy(msg.payload, frame_buf + header_sz, msg.payload_len);
                }
                
                /* Feed to router */
                deos_feed_message(&msg, DEOS_TRANSPORT_ETH_TEXTUAL);
            }
        }

        /* Clean up client connection */
        close(client);
        active_client_sock = -1;
    }
}

int eth_textual_init(void)
{
    /* Register this interface with the DEOS Router */
    deos_router_register_transport(DEOS_TRANSPORT_ETH_TEXTUAL, textual_tx_callback);

    /* Start the TX and RX/Server threads */
    k_thread_create(&textual_tx_thread_data, textual_tx_stack,
                    K_KERNEL_STACK_SIZEOF(textual_tx_stack),
                    textual_tx_thread, NULL, NULL, NULL,
                    TEXTUAL_THREAD_PRIO, 0, K_NO_WAIT);

    k_thread_create(&textual_server_thread_data, textual_server_stack,
                    K_KERNEL_STACK_SIZEOF(textual_server_stack),
                    textual_server_thread, NULL, NULL, NULL,
                    TEXTUAL_THREAD_PRIO, 0, K_NO_WAIT);

    LOG_INF("Textual Ethernet interface initialized");
    return 0;
}
