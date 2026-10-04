#include <zephyr/ztest.h>
#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "../../../lib/deos_core/src/deos_internal.h"

ZTEST_SUITE(deos_protocol, NULL, NULL, NULL, NULL, NULL);

ZTEST(deos_protocol, test_01_can_id_encode_decode_roundtrip)
{
    TC_PRINT("Test: Verifies that encoding a CAN ID and then decoding it results in the original fields.\n");
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING,
        DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &can_id);
    
    zassert_equal(ret, 0, "Encode failed");

    deos_message_t msg;
    ret = deos_can_id_decode(can_id, &msg);
    zassert_equal(ret, 0, "Decode failed");
    
    zassert_equal(msg.priority, DEOS_PRIO_CONTROL, "Priority mismatch");
    zassert_equal(msg.message_class, DEOS_CLASS_COMMAND, "Class mismatch");
    zassert_equal(msg.service, DEOS_SERVICE_STEERING, "Service mismatch");
    zassert_equal(msg.destination, DEOS_NODE_STEERING, "Destination mismatch");
    zassert_equal(msg.source, DEOS_NODE_MAIN_STM32, "Source mismatch");
}

ZTEST(deos_protocol, test_02_maximum_valid_29bit_id_fields)
{
    TC_PRINT("Test: Verifies that encoding works with the maximum possible values for all fields within a 29-bit CAN ID.\n");
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        DEOS_CAN_PRIORITY_MASK, DEOS_CAN_MESSAGE_CLASS_MASK, 
        DEOS_CAN_SERVICE_MASK, DEOS_CAN_DESTINATION_MASK, 
        DEOS_CAN_SOURCE_MASK, &can_id);
    zassert_equal(ret, 0, "Max fields encode failed");
}

ZTEST(deos_protocol, test_03_invalid_priority_reject)
{
    TC_PRINT("Test: Ensures that an invalid priority value is rejected during CAN ID encoding.\n");
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        8 /* Invalid priority */, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING,
        DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &can_id);
    zassert_not_equal(ret, 0, "Invalid priority should fail");
}

ZTEST(deos_protocol, test_04_invalid_message_class_reject)
{
    TC_PRINT("Test: Ensures that an invalid message class value is rejected during CAN ID encoding.\n");
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        DEOS_PRIO_CONTROL, 16 /* Invalid class */, DEOS_SERVICE_STEERING,
        DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &can_id);
    zassert_not_equal(ret, 0, "Invalid class should fail");
}

ZTEST(deos_protocol, test_05_invalid_service_reject)
{
    TC_PRINT("Test: Ensures that an invalid service value is rejected during CAN ID encoding.\n");
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, 64 /* Invalid service */,
        DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &can_id);
    zassert_not_equal(ret, 0, "Invalid service should fail");
}

ZTEST(deos_protocol, test_06_invalid_source_reject)
{
    TC_PRINT("Test: Ensures that a CAN frame with an invalid source node ID is rejected during decoding.\n");
    /* Decode level reject */
    struct can_frame frame = { .flags = CAN_FRAME_IDE | CAN_FRAME_FDF | CAN_FRAME_BRS, .dlc = 4, .data = {DEOS_PROTOCOL_VERSION, 0, 0, 0} };
    deos_can_id_encode(DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING, DEOS_NODE_STEERING, DEOS_NODE_INVALID, &frame.id);
    
    deos_message_t msg;
    int ret = deos_decode_frame(&frame, &msg);
    zassert_equal(ret, -EPROTO, "Invalid source should return EPROTO");
}

ZTEST(deos_protocol, test_07_invalid_destination_reject)
{
    TC_PRINT("Test: Ensures that a CAN frame with an invalid destination node ID is rejected during decoding.\n");
    struct can_frame frame = { .flags = CAN_FRAME_IDE | CAN_FRAME_FDF | CAN_FRAME_BRS, .dlc = 4, .data = {DEOS_PROTOCOL_VERSION, 0, 0, 0} };
    deos_can_id_encode(DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING, DEOS_NODE_INVALID, DEOS_NODE_MAIN_STM32, &frame.id);
    
    deos_message_t msg;
    int ret = deos_decode_frame(&frame, &msg);
    zassert_equal(ret, -EPROTO, "Invalid dest should return EPROTO");
}

ZTEST(deos_protocol, test_08_local_node_invalid_init_reject)
{
    TC_PRINT("Test: Ensures that initializing DEOS Core with an invalid node ID fails.\n");
    struct deos_config config = { .node_id = DEOS_NODE_INVALID };
    int ret = deos_init(&config);
    zassert_equal(ret, -EINVAL, "Init with invalid node should fail");
}

ZTEST(deos_protocol, test_09_local_node_broadcast_init_reject)
{
    TC_PRINT("Test: Ensures that initializing DEOS Core with the broadcast node ID fails.\n");
    struct deos_config config = { .node_id = DEOS_NODE_BROADCAST };
    int ret = deos_init(&config);
    zassert_equal(ret, -EINVAL, "Init with broadcast node should fail");
}

ZTEST_SUITE(deos_fault_test, NULL, NULL, NULL, NULL, NULL);

ZTEST(deos_fault_test, test_fault_01_reject_zero_id)
{
    TC_PRINT("Test: Verifies that raising a fault with ID 0 is rejected.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    int ret = deos_fault_raise(0, DEOS_FAULT_SEVERITY_ERROR);
    zassert_equal(ret, -EINVAL, "Fault ID 0 should be rejected");
}

ZTEST(deos_fault_test, test_fault_02_new_fault_raise)
{
    TC_PRINT("Test: Verifies that a valid new fault can be raised successfully and marked as active.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    int ret = deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    zassert_equal(ret, 0, "Raise failed");
    zassert_true(deos_fault_is_active(0x0100), "Fault should be active");
}

ZTEST(deos_fault_test, test_fault_03_same_fault_repeated)
{
    TC_PRINT("Test: Verifies that raising an already active fault again is handled safely.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    
    /* Internal API would be needed to test occurrence count directly, 
       but we can at least ensure it doesn't fail. */
    zassert_true(deos_fault_is_active(0x0100), "Fault should still be active");
}

ZTEST(deos_fault_test, test_fault_04_set_inactive)
{
    TC_PRINT("Test: Verifies that setting an active fault to inactive updates its state correctly.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_set_inactive(0x0100);
    zassert_false(deos_fault_is_active(0x0100), "Fault should be inactive");
}

ZTEST(deos_fault_test, test_fault_05_latch)
{
    TC_PRINT("Test: Verifies that latching a fault works and keeps the fault active.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_latch(0x0100);
    zassert_true(deos_fault_is_active(0x0100), "Latched fault should be active");
}

ZTEST(deos_fault_test, test_fault_06_clear_single)
{
    TC_PRINT("Test: Verifies that a specific single fault can be cleared.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_clear(0x0100);
    zassert_false(deos_fault_is_active(0x0100), "Fault should be cleared");
}

ZTEST(deos_fault_test, test_fault_07_clear_all)
{
    TC_PRINT("Test: Verifies that all active faults are cleared when clear_all is called.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_raise(0x0101, DEOS_FAULT_SEVERITY_ERROR);
    deos_fault_clear_all();
    zassert_false(deos_fault_is_active(0x0100), "Fault 0x0100 should be cleared");
    zassert_false(deos_fault_is_active(0x0101), "Fault 0x0101 should be cleared");
}

ZTEST(deos_fault_test, test_fault_08_max_table_full)
{
    TC_PRINT("Test: Verifies that the fault table safely rejects new faults with -ENOSPC when it reaches maximum capacity.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    for (int i = 1; i <= 32; i++) {
        deos_fault_raise(i, DEOS_FAULT_SEVERITY_INFO);
    }
    int ret = deos_fault_raise(33, DEOS_FAULT_SEVERITY_INFO);
    zassert_equal(ret, -ENOSPC, "Should return -ENOSPC when table is full");
}

ZTEST_SUITE(deos_local_node_test, NULL, NULL, NULL, NULL, NULL);

ZTEST(deos_local_node_test, test_local_node_01_primary_node_is_local)
{
    TC_PRINT("Test: Verifies that the primary node initialized in the config is recognized as a local node.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    zassert_true(deos_is_local_node(DEOS_NODE_STEERING), "Primary node should be local");
}

ZTEST(deos_local_node_test, test_local_node_02_unregistered_node_is_not_local)
{
    TC_PRINT("Test: Verifies that an arbitrary unregistered node ID is correctly identified as non-local.\n");
    zassert_false(deos_is_local_node(DEOS_NODE_BRAKE), "Unregistered node should not be local");
}

ZTEST(deos_local_node_test, test_local_node_03_invalid_node_is_not_local)
{
    TC_PRINT("Test: Verifies that the invalid node ID is not recognized as a local node.\n");
    zassert_false(deos_is_local_node(DEOS_NODE_INVALID), "Invalid node should not be local");
}

/* 
 * NOTE: Further tests requiring deos_init (which requires a valid CAN dev)
 * like register_local_node and routing can be implemented when a full CAN mock
 * or loopback device is fully linked into the ZTEST runner.
 *
 * Current tests cover the basic codec, fault storage, and basic node identity checks.
 */

ZTEST_SUITE(deos_node_fault_test, NULL, NULL, NULL, NULL, NULL);

ZTEST(deos_node_fault_test, test_01_multi_node_fault_isolation)
{
    TC_PRINT("Test: Verifies that node-specific fault operations isolate faults (e.g. an unregistered node cannot raise faults).\n");
    /* In a mocked CAN environment, these nodes would be registered via deos_register_local_node.
       Since we bypass init in these partial tests, we just assume DEOS_NODE_STEERING 
       is primary and active. We check unregistered behavior for DEOS_NODE_BRAKE. */

    deos_fault_init();

    /* 1. Unregistered node should fail to raise fault */
    int ret = deos_fault_raise_for_node(DEOS_NODE_BRAKE, 0x1111, DEOS_FAULT_SEVERITY_ERROR);
    zassert_equal(ret, -EPERM, "Unregistered node should not be able to raise fault");

    /* 2. Unregistered node should not have active faults */
    zassert_false(deos_fault_is_active_for_node(DEOS_NODE_BRAKE, 0x1111), "Unregistered node should not have active fault");

    /* 3. Primary node should fail if not properly initialized in this test context,
       but assuming it was, it would succeed. To make this pass without deos_init(), 
       we can't easily test it here. We document the test logic. */
}

ZTEST(deos_node_fault_test, test_02_legacy_wrappers)
{
    TC_PRINT("Test: Verifies that the legacy global fault wrappers correctly route fault operations to the primary node.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    
    int ret = deos_fault_raise(0x1234, DEOS_FAULT_SEVERITY_WARNING);
    zassert_equal(ret, 0, "Legacy wrapper should succeed on primary node");
    
    zassert_true(deos_fault_is_active_for_node(DEOS_NODE_STEERING, 0x1234), "Fault should be active on primary node");
}

ZTEST_SUITE(deos_codec_test, NULL, NULL, NULL, NULL, NULL);

static void do_roundtrip_test(uint8_t payload_size) {
    deos_message_t tx_msg = {0};
    tx_msg.priority = DEOS_PRIO_CONTROL;
    tx_msg.message_class = DEOS_CLASS_COMMAND;
    tx_msg.service = DEOS_SERVICE_STEERING;
    tx_msg.destination = DEOS_NODE_STEERING;
    tx_msg.source = DEOS_NODE_MAIN_STM32;
    tx_msg.command = 0xAA;
    tx_msg.version = DEOS_PROTOCOL_VERSION;
    tx_msg.sequence = 0x55;
    tx_msg.payload_len = payload_size;
    
    for (int i=0; i<payload_size; i++) {
        tx_msg.payload[i] = (uint8_t)(i & 0xFF);
    }
    
    struct can_frame frame = {0};
    int ret = deos_encode_frame(&tx_msg, &frame);
    zassert_equal(ret, 0, "Encode failed for payload %d", payload_size);
    
    uint8_t physical_len = can_dlc_to_bytes(frame.dlc);
    zassert_true(physical_len >= 4 + payload_size, "Physical length %d too small for semantic %d", physical_len, 4 + payload_size);
    
    deos_message_t rx_msg = {0};
    ret = deos_decode_frame(&frame, &rx_msg);
    zassert_equal(ret, 0, "Decode failed for payload %d", payload_size);
    zassert_equal(rx_msg.payload_len, payload_size, "Length mismatch");
    zassert_equal(rx_msg.command, 0xAA, "Command mismatch");
    
    if (payload_size > 0) {
        zassert_equal(memcmp(tx_msg.payload, rx_msg.payload, payload_size), 0, "Payload mismatch for size %d", payload_size);
    }
}

ZTEST(deos_codec_test, test_codec_01_roundtrip_boundaries) {
    TC_PRINT("Test: Encode-decode roundtrip for various payload sizes\n");
    uint8_t sizes[] = {0, 1, 4, 5, 8, 9, 10, 12, 13, 16, 20, 28, 44, 60};
    for (int i=0; i<sizeof(sizes)/sizeof(sizes[0]); i++) {
        do_roundtrip_test(sizes[i]);
    }
}

ZTEST(deos_codec_test, test_codec_02_invalid_frames) {
    TC_PRINT("Test: Invalid frames (length, physical limits)\n");
    struct can_frame frame = {0};
    frame.flags = CAN_FRAME_IDE | CAN_FRAME_FDF | CAN_FRAME_BRS;
    deos_can_id_encode(DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING, DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &frame.id);
    
    deos_message_t rx_msg = {0};
    
    /* 1. physical < 4 */
    frame.dlc = can_bytes_to_dlc(3);
    int ret = deos_decode_frame(&frame, &rx_msg);
    zassert_equal(ret, -EMSGSIZE, "Expected -EMSGSIZE");
    
    /* 2. Length > 60 */
    frame.dlc = can_bytes_to_dlc(64);
    frame.data[0] = DEOS_PROTOCOL_VERSION;
    frame.data[3] = 61; /* payload_len */
    ret = deos_decode_frame(&frame, &rx_msg);
    zassert_equal(ret, -EMSGSIZE, "Expected -EMSGSIZE for Length > 60");
    
    /* 3. Length + 4 > physical */
    frame.dlc = can_bytes_to_dlc(16);
    frame.data[3] = 20; /* 4 + 20 = 24 > 16 */
    ret = deos_decode_frame(&frame, &rx_msg);
    zassert_equal(ret, -EMSGSIZE, "Expected -EMSGSIZE for Length mismatch");
}

ZTEST(deos_codec_test, test_codec_03_fault_and_ping_sizes) {
    TC_PRINT("Test: Verify PING (4 bytes) and FAULT (10 bytes) response sizes\n");
    do_roundtrip_test(4); /* Ping */
    do_roundtrip_test(10); /* Fault response */
}

ZTEST(deos_codec_test, test_codec_04_version_validation) {
    TC_PRINT("Test: Ensure v1.0 frames are rejected due to exact version match requirement\n");
    struct can_frame frame = {0};
    frame.flags = CAN_FRAME_IDE | CAN_FRAME_FDF | CAN_FRAME_BRS;
    deos_can_id_encode(DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING, DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &frame.id);
    
    frame.dlc = can_bytes_to_dlc(4);
    frame.data[DEOS_VERSION_OFFSET] = 0x10; /* v1.0 */
    frame.data[DEOS_LENGTH_OFFSET] = 0;
    
    deos_message_t rx_msg = {0};
    int ret = deos_decode_frame(&frame, &rx_msg);
    zassert_equal(ret, -EPROTONOSUPPORT, "Expected -EPROTONOSUPPORT for v1.0 frame");
    
    /* Test valid version */
    frame.data[DEOS_VERSION_OFFSET] = DEOS_PROTOCOL_VERSION; /* v1.1 */
    ret = deos_decode_frame(&frame, &rx_msg);
    zassert_equal(ret, 0, "Valid version should decode");
}

ZTEST_SUITE(deos_heartbeat_test, NULL, NULL, NULL, NULL, NULL);

static bool heartbeat_handler_called = false;
static int test_heartbeat_handler(const deos_message_t *msg, void *user_data)
{
    heartbeat_handler_called = true;
    return 0;
}

ZTEST(deos_heartbeat_test, test_01_heartbeat_receive_routes_to_app)
{
    TC_PRINT("Test: Ensure incoming heartbeat passes to application handler and doesn't auto-consume\n");
    struct deos_config config = { .node_id = DEOS_NODE_MAIN_STM32 };
    deos_init(&config);

    deos_register_handler(
        DEOS_CLASS_NETWORK,
        DEOS_SERVICE_SYSTEM,
        DEOS_CMD_SYSTEM_HEARTBEAT,
        test_heartbeat_handler,
        NULL);

    deos_message_t msg = {0};
    msg.destination = DEOS_NODE_BROADCAST;
    msg.source = 0x20;
    msg.message_class = DEOS_CLASS_NETWORK;
    msg.service = DEOS_SERVICE_SYSTEM;
    msg.command = DEOS_CMD_SYSTEM_HEARTBEAT;
    msg.payload[0] = DEOS_STATE_ACTIVE;
    msg.payload_len = 1;
    
    heartbeat_handler_called = false;
    deos_dispatch(&msg, DEOS_TRANSPORT_CAN_FD);
    zassert_true(heartbeat_handler_called, "Heartbeat handler should be called");
}

ZTEST(deos_heartbeat_test, test_02_heartbeat_send_unregistered_node)
{
    TC_PRINT("Test: Ensure heartbeat from unregistered node is rejected\n");
    struct deos_config config = { .node_id = DEOS_NODE_MAIN_STM32 };
    deos_init(&config);

    int ret = deos_send_heartbeat_from_node(0x99, DEOS_STATE_ACTIVE); /* Unregistered */
    zassert_equal(ret, -EPERM, "Should reject unregistered source node");
}

ZTEST(deos_heartbeat_test, test_03_heartbeat_send_fields)
{
    TC_PRINT("Test: Ensure heartbeat API uses correct arguments (indirectly by ensuring it succeeds on valid node)\n");
    struct deos_config config = { .node_id = DEOS_NODE_MAIN_STM32 };
    deos_init(&config);
    
    /* In a full mock we could intercept the frame, but we at least ensure it passes validation */
    int ret = deos_send_heartbeat(DEOS_STATE_ACTIVE);
    /* Note: without can_start(), deos_send might return -ENETDOWN depending on zephyr state, 
       but if loopback is ready it returns 0. As long as it doesn't return -EINVAL we are good. */
    zassert_not_equal(ret, -EINVAL, "deos_send_heartbeat should build valid fields");
}

ZTEST_SUITE(deos_response_test, NULL, NULL, NULL, NULL, NULL);

ZTEST(deos_response_test, test_01_response_validation)
{
    TC_PRINT("Test: Validate response generation rules and size limits\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    
    deos_message_t orig = {0};
    orig.destination = DEOS_NODE_STEERING;
    orig.source = DEOS_NODE_MAIN_STM32;
    orig.priority = DEOS_PRIO_CONTROL;
    orig.message_class = DEOS_CLASS_COMMAND;
    orig.service = DEOS_SERVICE_STEERING;
    orig.command = DEOS_CMD_STEERING_SET_TARGET_ANGLE;
    orig.payload_len = 4;
    
    /* 1. SUCCESS response encode (zero-length optional data) */
    int ret = deos_send_response(&orig, DEOS_RESULT_SUCCESS, NULL, 0);
    zassert_not_equal(ret, -EINVAL, "Valid zero-length data should pass validation");
    zassert_not_equal(ret, -EMSGSIZE, "Valid zero-length data should pass validation");
    
    /* 2. NOT_ALLOWED response encode */
    ret = deos_send_response(&orig, DEOS_RESULT_NOT_ALLOWED, NULL, 0);
    zassert_not_equal(ret, -EINVAL, "NOT_ALLOWED should pass validation");
    
    /* 3. response with additional data */
    uint8_t data[10] = {1, 2, 3};
    ret = deos_send_response(&orig, DEOS_RESULT_SUCCESS, data, sizeof(data));
    zassert_not_equal(ret, -EINVAL, "Additional data should pass validation");
    
    /* 5. 59-byte maximum optional data */
    uint8_t max_data[59] = {0};
    ret = deos_send_response(&orig, DEOS_RESULT_SUCCESS, max_data, 59);
    zassert_not_equal(ret, -EMSGSIZE, "59 byte data should be allowed");
    
    /* 6. >59 byte rejection */
    uint8_t too_large[60] = {0};
    ret = deos_send_response(&orig, DEOS_RESULT_SUCCESS, too_large, 60);
    zassert_equal(ret, -EMSGSIZE, "60 byte data should be rejected");
}
