# DEOS Core Architecture

## Overview
DEOS Core is a library that acts as the communication protocol stack for STM32 nodes. It translates CAN-FD frames into structured `deos_message_t` events and dispatches them to application callbacks.

## Data Flow Diagrams

### RX Flow
```text
CAN-FD Bus
    |
    v
Zephyr CAN RX Callback (Interrupt Context)
    |
    v (k_msgq_put)
Static RX Queue
    |
    v (k_msgq_get)
DEOS RX Thread (k_thread)
    |
    v
DEOS Decoder (deos_decode_frame)
    |
    v
Dispatcher (deos_dispatch)
    |
    +----------> Router (deos_route_message, e.g., for Main STM32)
    |
    +----------> Common DEOS (deos_network.c)
    |               |
    |               +--> PING (Generates PONG automatically)
    |               +--> HEARTBEAT
    |
    +----------> Application Handler (Registered via deos_register_handler)
```

### TX Flow
```text
Application (e.g., Steering Control) / Common DEOS (e.g., PONG generator)
        |
        v
deos_send() / deos_send_ping()
        |
        v
DEOS Encoder (deos_encode_frame & deos_can_id_encode)
        |
        v
CAN-FD Frame (struct can_frame)
        |
        v
Zephyr CAN API (can_send)
        |
        v
CAN-FD Bus
```

## Threading Model
- **DEOS Core is not a monolithic thread.** It operates as a library with a static background worker (`DEOS RX Thread`) responsible solely for pulling packets off the CAN bus queue, decoding, and dispatching.
- **Application Logic is separate.** Applications must spawn their own threads for continuous work (like PID controllers or motor driver tasks) and only interact with DEOS via message payloads asynchronously. This guarantees that `DEOS RX Thread` is never blocked by mechanical or algorithmic delays.
