#ifndef DALY_PARSER_H
#define DALY_PARSER_H

#include <stdint.h>

struct DalyPackInfo {
    uint16_t voltage_dV; // 0.1 V
    int32_t current_dA;  // 0.1 A
    uint16_t soc_dPct;   // 0.1 %
};

struct DalyCellInfo {
    uint8_t frame_index;
    uint16_t cell_v_mV[3]; // mV
};

struct DalyBalancingState {
    uint8_t state; // 0=OFF, 1=PASSIVE, 2=ACTIVE
};

struct DalyBalancingCells {
    uint32_t cell_mask; // Bit 0 = Cell 1, ..., Bit 19 = Cell 20
};

struct DalyAlarmInfo {
    bool pack_overvoltage;
    bool pack_undervoltage;
    bool charge_overcurrent;
    bool discharge_overcurrent;
    bool overtemperature;
    bool undertemperature;
    bool bms_internal_fault;
};

class DalyParser {
public:
    static bool parsePackInfo(const uint8_t *data, DalyPackInfo &info);
    static bool parseCellInfo(const uint8_t *data, DalyCellInfo &info);
    static bool parseBalancingState(const uint8_t *data, DalyBalancingState &info);
    static bool parseBalancingCells(const uint8_t *data, DalyBalancingCells &info);
    static bool parseAlarms(const uint8_t *data, DalyAlarmInfo &info);
};

#endif /* DALY_PARSER_H */
