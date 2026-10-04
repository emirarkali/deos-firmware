#pragma once

#include "ITransport.hpp"
#include <zephyr/kernel.h>

class EthUros : public ITransport {
public:
    static EthUros& getInstance();

    int init() override;
    int send(const deos_message_t* msg) override;

private:
    EthUros() = default;
    ~EthUros() = default;

    // Non-copyable
    EthUros(const EthUros&) = delete;
    EthUros& operator=(const EthUros&) = delete;

    static int tx_callback(const deos_message_t *msg);
};
