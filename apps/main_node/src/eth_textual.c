#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "eth_textual.h"

LOG_MODULE_REGISTER(eth_textual, LOG_LEVEL_INF);

#define TEXTUAL_TX_QUEUE_SIZE 16
#define TEXTUAL_THREAD_STACK_SIZE 1024
#define TEXTUAL_THREAD_PRIO 5

/* TX Queue to prevent blocking the router */
K_MSGQ_DEFINE(textual_tx_q, sizeof(deos_message_t), TEXTUAL_TX_QUEUE_SIZE, 4);

/* Threads */
static struct k_thread textual_rx_thread_data;
static K_KERNEL_STACK_DEFINE(textual_rx_stack, TEXTUAL_THREAD_STACK_SIZE);

static struct k_thread textual_tx_thread_data;
static K_KERNEL_STACK_DEFINE(textual_tx_stack, TEXTUAL_THREAD_STACK_SIZE);

/* Router TX callback */
static int textual_tx_callback(const deos_message_t *msg)
{
    /* Push to queue, do not block the router */
    if (k_msgq_put(&textual_tx_q, msg, K_NO_WAIT) != 0) {
        LOG_WRN("Textual TX queue full, dropping message");
        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        return -ENOSPC;
    }
    return 0;
}

/* TX Thread (Sends data out over UDP) */
static void textual_tx_thread(void *p1, void *p2, void *p3)
{
    deos_message_t msg;
    LOG_INF("Textual TX Thread started");

    while (1) {
        if (k_msgq_get(&textual_tx_q, &msg, K_FOREVER) == 0) {
            /* TODO: Serialize msg to bytes and send via UDP socket */
            LOG_DBG("Textual TX: Sending message (Cmd: 0x%02X)", msg.command);
        }
    }
}

/* RX Thread (Receives data from UDP) */
static void textual_rx_thread(void *p1, void *p2, void *p3)
{
    LOG_INF("Textual RX Thread started");

    while (1) {
        /* TODO: Block on recvfrom(), parse bytes into msg */
        
        /* 
         * deos_message_t msg;
         * // parse bytes into msg...
         * 
         * deos_feed_message(&msg, DEOS_TRANSPORT_ETH_TEXTUAL);
         */
         
        k_sleep(K_MSEC(1000)); /* Placeholder to prevent tight loop until implemented */
    }
}

int eth_textual_init(void)
{
    /* Register this interface with the DEOS Router */
    deos_router_register_transport(DEOS_TRANSPORT_ETH_TEXTUAL, textual_tx_callback);

    /* Start the TX and RX threads */
    k_thread_create(&textual_tx_thread_data, textual_tx_stack,
                    K_KERNEL_STACK_SIZEOF(textual_tx_stack),
                    textual_tx_thread, NULL, NULL, NULL,
                    TEXTUAL_THREAD_PRIO, 0, K_NO_WAIT);

    k_thread_create(&textual_rx_thread_data, textual_rx_stack,
                    K_KERNEL_STACK_SIZEOF(textual_rx_stack),
                    textual_rx_thread, NULL, NULL, NULL,
                    TEXTUAL_THREAD_PRIO, 0, K_NO_WAIT);

    LOG_INF("Textual Ethernet interface initialized");
    return 0;
}
