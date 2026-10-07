#pragma once

#include "../ITransport.hpp"
#include <zephyr/kernel.h>

class EthTextual : public ITransport {
public:
    static EthTextual& getInstance();

    int init() override;
    int send(const deos_message_t* msg) override;

private:
    EthTextual();
    ~EthTextual() = default;

    // Non-copyable
    EthTextual(const EthTextual&) = delete;
    EthTextual& operator=(const EthTextual&) = delete;

    void tx_thread();
    void server_thread();

    static void tx_thread_entry(void *p1, void *p2, void *p3);
    static void server_thread_entry(void *p1, void *p2, void *p3);
    static int tx_callback(const deos_message_t *msg);

    int m_active_client_sock;
    struct k_msgq m_tx_q;
    char m_tx_q_buffer[16 * sizeof(deos_message_t)]; // TEXTUAL_TX_QUEUE_SIZE = 16
};
