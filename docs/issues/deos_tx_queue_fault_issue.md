# Issue: `DEOS_FAULT_TX_FAILURE` Not Raised on Router RX/TX Path

## Description
When the `DEOS_NODE_MAIN_STM32` (or any node with `config->router_enabled = true`) attempts to send or route a CAN packet, the message is dispatched through `deos_dispatch` and subsequently `deos_internal_canfd_tx`.

If the `tx_msgq` (CAN TX Queue) is full, `deos_internal_canfd_tx` correctly drops the message and logs `<wrn> deos_tx: TX Queue full, dropping message`, returning `-ENOSPC`.

However, unlike the fallback path in `deos_send_from_node` (which explicitly calls `deos_fault_raise(DEOS_FAULT_TX_FAILURE, ...)` when `k_msgq_put` fails), the `deos_internal_canfd_tx` function **does not raise any fault**.

As a result, even though the system is continuously dropping outgoing CAN frames due to queue saturation (as seen in the logs), querying the node via `GET_FAULTS` returns an empty fault list because no fault was ever formally raised in the `deos_fault` subsystem.

## Proposed Solution
Modify `lib/deos_core/src/deos_tx.c`:
Inside `deos_internal_canfd_tx(const deos_message_t *msg)`, immediately after the warning log, add the `deos_fault_raise` call.

```c
int deos_internal_canfd_tx(const deos_message_t *msg)
{
    if (k_msgq_put(&tx_msgq, msg, K_NO_WAIT) != 0) {
        LOG_WRN("TX Queue full, dropping message");
        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        return -ENOSPC;
    }
    return 0;
}
```

Since changes to `lib/` are strictly regulated and AI agents are blocked from committing to `lib/deos_core/`, this issue is documented here for Emir Arkalı's approval and manual intervention.
