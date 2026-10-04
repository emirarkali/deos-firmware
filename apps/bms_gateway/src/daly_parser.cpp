#include "daly_parser.h"

static uint16_t get_be16(const uint8_t *data) {
    return (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
}

bool DalyParser::parsePackInfo(const uint8_t *data, DalyPackInfo &info) {
    info.voltage_dV = get_be16(&data[0]);
    info.current_dA = static_cast<int32_t>(get_be16(&data[4])) - 30000;
    info.soc_dPct = get_be16(&data[6]);
    return true;
}

bool DalyParser::parseCellInfo(const uint8_t *data, DalyCellInfo &info) {
    info.frame_index = data[0];
    info.cell_v_mV[0] = get_be16(&data[1]);
    info.cell_v_mV[1] = get_be16(&data[3]);
    info.cell_v_mV[2] = get_be16(&data[5]);
    return true;
}

bool DalyParser::parseBalancingState(const uint8_t *data, DalyBalancingState &info) {
    uint16_t state = get_be16(&data[0]);
    info.state = static_cast<uint8_t>(state);
    return true;
}

bool DalyParser::parseBalancingCells(const uint8_t *data, DalyBalancingCells &info) {
    uint16_t reg_4f = get_be16(&data[0]);
    uint16_t reg_50 = get_be16(&data[2]);
    // reg_51 is in data[4], data[5] but not needed for 20S

    info.cell_mask = static_cast<uint32_t>(reg_4f) | (static_cast<uint32_t>(reg_50 & 0x000F) << 16);
    return true;
}

bool DalyParser::parseAlarms(const uint8_t *data, DalyAlarmInfo &info) {
    // Byte 0
    info.pack_overvoltage = (data[0] & (1 << 0)) || (data[0] & (1 << 2)); // Cell or Pack OV
    info.pack_undervoltage = (data[0] & (1 << 1)) || (data[0] & (1 << 3)); // Cell or Pack UV
    info.overtemperature = (data[0] & (1 << 4)) || (data[0] & (1 << 6)); // Charge or Discharge OT
    info.undertemperature = (data[0] & (1 << 5)) || (data[0] & (1 << 7)); // Charge or Discharge UT

    // Byte 1
    info.charge_overcurrent = (data[1] & (1 << 0));
    info.discharge_overcurrent = (data[1] & (1 << 1));
    info.bms_internal_fault = (data[1] & (1 << 2)) || (data[1] & (1 << 3)); // Short circuit or IC error

    return true;
}
