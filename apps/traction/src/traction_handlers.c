#include "traction_app.h"
#include "traction_config.h"
#include "traction_hw.h"
#include <deos/deos.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <string.h>

LOG_MODULE_REGISTER(traction_handlers, LOG_LEVEL_INF);

static int handle_set_target_speed(const deos_message_t *msg, void *user_data)
{
    if (msg->payload_len != sizeof(struct deos_traction_target_payload)) {
        return DEOS_RESULT_INVALID_PARAMETER;
    }
    
    const struct deos_traction_target_payload *payload = 
        (const struct deos_traction_target_payload *)msg->payload;
        
    g_traction_state.target_speed_raw = payload->target_speed;
    g_traction_state.target_accel_raw = payload->target_accel;
    g_traction_state.last_control_command_ms = k_uptime_get();
    
    return DEOS_RESULT_SUCCESS;
}

static int handle_set_gear(const deos_message_t *msg, void *user_data)
{
    if (msg->payload_len != sizeof(struct deos_traction_gear_payload)) {
        return DEOS_RESULT_INVALID_PARAMETER;
    }
    
    const struct deos_traction_gear_payload *payload = 
        (const struct deos_traction_gear_payload *)msg->payload;
        
    // Validate gear change (e.g. speed must be near 0 for reverse)
    if (payload->gear == DEOS_GEAR_REVERSE && g_traction_state.current_speed_raw > 10) {
        return DEOS_RESULT_NOT_ALLOWED;
    }
    
    g_traction_state.target_gear = payload->gear;
    traction_hw_set_gear(payload->gear);
    
    return DEOS_RESULT_SUCCESS;
}

static int handle_set_torque_limit(const deos_message_t *msg, void *user_data)
{
    if (msg->payload_len != sizeof(struct deos_traction_torque_limit_payload)) {
        return DEOS_RESULT_INVALID_PARAMETER;
    }
    
    const struct deos_traction_torque_limit_payload *payload = 
        (const struct deos_traction_torque_limit_payload *)msg->payload;
        
    g_traction_state.torque_limit_raw = payload->torque_limit;
    
    return DEOS_RESULT_SUCCESS;
}

static int handle_get_status(const deos_message_t *msg, void *user_data)
{
    struct traction_status_response_payload resp;
    resp.current_speed = g_traction_state.current_speed_raw;
    resp.current_accel = g_traction_state.current_accel_raw;
    resp.current_gear = g_traction_state.target_gear;
    resp.torque_limit = g_traction_state.torque_limit_raw;
    resp.state_flags = 0;
    
    if (g_traction_state.emergency_stop) resp.state_flags |= 0x01;
    if (g_traction_state.brake_active) resp.state_flags |= 0x02;
    
    deos_send(
        msg->source,
        DEOS_PRIO_STATUS,
        DEOS_CLASS_RESPONSE,
        DEOS_SERVICE_TRACTION,
        DEOS_CMD_TRACTION_GET_STATUS,
        &resp,
        sizeof(resp)
    );
    
    return DEOS_RESULT_SUCCESS;
}

// Handler for listening to Brake status
static int handle_brake_status(const deos_message_t *msg, void *user_data)
{
    // Assuming byte 0 of Brake status is active flag (dummy logic for cross-node input)
    if (msg->payload_len > 0) {
        g_traction_state.brake_active = (msg->payload[0] != 0);
    }
    return DEOS_RESULT_SUCCESS;
}

// Handler for listening to BMS status
static int handle_bms_status(const deos_message_t *msg, void *user_data)
{
    // Dummy parsing for BMS SOC etc based on some generic payload assumption
    // In real system, this should use deos_bms_status_payload from deos_types.h
    return DEOS_RESULT_SUCCESS;
}

static int handle_get_parameter(const deos_message_t *msg, void *user_data)
{
    if (msg->payload_len < 2) return DEOS_RESULT_INVALID_PARAMETER;
    
    uint16_t param_id;
    memcpy(&param_id, msg->payload, sizeof(uint16_t));
    
    uint8_t resp_buf[64];
    memcpy(resp_buf, &param_id, sizeof(uint16_t));
    
    size_t out_len = 0;
    int ret = traction_config_get_param((deos_traction_parameter_t)param_id, resp_buf + 2, &out_len);
    
    if (ret == 0) {
        deos_send(
            msg->source,
            DEOS_PRIO_CONFIG,
            DEOS_CLASS_RESPONSE,
            DEOS_SERVICE_TRACTION,
            DEOS_CMD_GET_PARAMETER, // Common config service response
            resp_buf,
            out_len + 2
        );
        return DEOS_RESULT_SUCCESS;
    }
    return DEOS_RESULT_INVALID_PARAMETER;
}

static int handle_set_parameter(const deos_message_t *msg, void *user_data)
{
    if (msg->payload_len < 2) return DEOS_RESULT_INVALID_PARAMETER;
    
    uint16_t param_id;
    memcpy(&param_id, msg->payload, sizeof(uint16_t));
    
    int ret = traction_config_set_param(
        (deos_traction_parameter_t)param_id, 
        msg->payload + 2, 
        msg->payload_len - 2
    );
    
    return (ret == 0) ? DEOS_RESULT_SUCCESS : DEOS_RESULT_INVALID_PARAMETER;
}

void traction_handlers_init(void)
{
    // Command Handlers (TRACTION)
    deos_register_handler(DEOS_CLASS_COMMAND, DEOS_SERVICE_TRACTION, DEOS_CMD_TRACTION_SET_TARGET_SPEED, handle_set_target_speed, NULL);
    deos_register_handler(DEOS_CLASS_COMMAND, DEOS_SERVICE_TRACTION, DEOS_CMD_TRACTION_SET_GEAR, handle_set_gear, NULL);
    deos_register_handler(DEOS_CLASS_COMMAND, DEOS_SERVICE_TRACTION, DEOS_CMD_TRACTION_SET_TORQUE_LIMIT, handle_set_torque_limit, NULL);
    
    // Request Handlers (TRACTION)
    deos_register_handler(DEOS_CLASS_REQUEST, DEOS_SERVICE_TRACTION, DEOS_CMD_TRACTION_GET_STATUS, handle_get_status, NULL);
    
    // Config Handlers (TRACTION)
    deos_register_handler(DEOS_CLASS_CONFIG, DEOS_SERVICE_TRACTION, DEOS_CMD_GET_PARAMETER, handle_get_parameter, NULL);
    deos_register_handler(DEOS_CLASS_CONFIG, DEOS_SERVICE_TRACTION, DEOS_CMD_SET_PARAMETER, handle_set_parameter, NULL);
    
    // Cross-node Status Listeners
    // For brake, we can listen to response/status class, brake service, get_status command
    deos_register_handler(DEOS_CLASS_RESPONSE, DEOS_SERVICE_BRAKE, DEOS_CMD_BRAKE_GET_STATUS, handle_brake_status, NULL);
    deos_register_handler(DEOS_CLASS_RESPONSE, DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_STATUS, handle_bms_status, NULL);
    
    LOG_INF("Traction handlers registered");
}
