#ifndef BMS_GATEWAY_H
#define BMS_GATEWAY_H

#include <stdint.h>

/**
 * @brief BMS Node-specific Fault IDs (Starts from 0x0100)
 */
typedef enum {
    DEOS_BMS_FAULT_PACK_OVERVOLTAGE      = 0x0100,
    DEOS_BMS_FAULT_PACK_UNDERVOLTAGE     = 0x0101,
    DEOS_BMS_FAULT_CHARGE_OVERCURRENT    = 0x0102,
    DEOS_BMS_FAULT_DISCHARGE_OVERCURRENT = 0x0103,
    DEOS_BMS_FAULT_OVERTEMPERATURE       = 0x0104,
    DEOS_BMS_FAULT_UNDERTEMPERATURE      = 0x0105,
    DEOS_BMS_FAULT_CELL_IMBALANCE        = 0x0106,
    DEOS_BMS_FAULT_INTERNAL_ERROR        = 0x0107,
    DEOS_BMS_FAULT_INVALID_MEASUREMENT   = 0x0108,
} deos_bms_node_fault_id_t;

/**
 * @brief BMS Gateway-specific Fault IDs (Starts from 0x0100)
 */
typedef enum {
    DEOS_GW_FAULT_MAIN_COMM_LOST = 0x0100,
    DEOS_GW_FAULT_AUX_COMM_LOST  = 0x0101,
    DEOS_GW_FAULT_CAN_BUS_OFF    = 0x0102,
    DEOS_GW_FAULT_FD_BUS_OFF     = 0x0103,
    DEOS_GW_FAULT_INVALID_FRAME  = 0x0104,
    DEOS_GW_FAULT_PROTOCOL_ERROR = 0x0105,
} deos_gw_fault_id_t;

/**
 * @brief Run the BMS Gateway application.
 * 
 * This function initializes the CAN interface and runs the 
 * Gateway proxy logic between DEOS CAN-FD and Classic CAN.
 * 
 * @return int 0 on success, negative on error.
 */
int run_bms_gateway(void);

#endif /* BMS_GATEWAY_H */
