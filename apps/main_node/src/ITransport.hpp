#pragma once

#include <deos/deos.h>

class ITransport {
public:
    virtual ~ITransport() = default;
    
    /**
     * @brief Initialize the transport.
     * @return 0 on success, negative error code on failure.
     */
    virtual int init() = 0;
    
    /**
     * @brief Send a message over the transport.
     * @param msg Pointer to the DEOS message to send.
     * @return 0 on success, negative error code on failure.
     */
    virtual int send(const deos_message_t* msg) = 0;
};
