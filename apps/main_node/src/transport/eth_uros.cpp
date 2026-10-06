#include "eth_uros.hpp"
#include <zephyr/logging/log.h>
#include <deos/deos.h>
#include <zephyr/kernel.h>

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/uint8_multi_array.h>
#include <rmw_microros/rmw_microros.h>

LOG_MODULE_REGISTER(eth_uros, LOG_LEVEL_INF);

// Define ROS 2 components
static rcl_allocator_t allocator;
static rclc_support_t support;
static rcl_node_t node;
static rcl_publisher_t publisher;
static rcl_subscription_t subscriber;
static rclc_executor_t executor;

static std_msgs__msg__UInt8MultiArray pub_msg;
static std_msgs__msg__UInt8MultiArray sub_msg;
static uint8_t pub_msg_buffer[256];
static uint8_t sub_msg_buffer[256];

// Thread for executor
K_THREAD_STACK_DEFINE(uros_stack, 4096);
static struct k_thread uros_thread;

EthUros& EthUros::getInstance() {
    static EthUros instance;
    return instance;
}

int EthUros::tx_callback(const deos_message_t *msg) {
    return getInstance().send(msg);
}

int EthUros::send(const deos_message_t *msg) {
    if (!msg) return -1;
    
    // Serialize DEOS message to ROS 2 multi array
    pub_msg.data.data = pub_msg_buffer;
    pub_msg.data.size = sizeof(deos_message_t);
    pub_msg.data.capacity = sizeof(pub_msg_buffer);
    memcpy(pub_msg_buffer, msg, sizeof(deos_message_t));

    rcl_ret_t ret = rcl_publish(&publisher, &pub_msg, NULL);
    if (ret != RCL_RET_OK) {
        LOG_ERR("Failed to publish micro-ROS message");
        return -1;
    }
    return 0;
}

// Subscription callback
void subscription_callback(const void * msgin) {
    const std_msgs__msg__UInt8MultiArray * msg = (const std_msgs__msg__UInt8MultiArray *)msgin;
    LOG_INF("Received ROS message with size: %zu", msg->data.size);
    if (msg->data.size == sizeof(deos_message_t)) {
        // Decode back to deos_message_t and route it locally if needed
        // const deos_message_t* rx_msg = (const deos_message_t*)msg->data.data;
        // e.g. deos_rx_queue_push(rx_msg);
    }
}

// Thread entry point
void uros_executor_thread(void *p1, void *p2, void *p3) {
    while (true) {
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
        k_msleep(10); // yield to other Zephyr threads
    }
}

int EthUros::init() {
    // 1. Initialize micro-ROS transport 
    // The Zephyr UDP module implicitly configures UDP sockets for uROS.

    // 2. Initialize rclc allocator
    allocator = rcl_get_default_allocator();

    // 3. Initialize support object
    rcl_ret_t ret = rclc_support_init(&support, 0, NULL, &allocator);
    if (ret != RCL_RET_OK) {
        LOG_ERR("Failed to initialize rclc support");
        return -1;
    }

    // 4. Create Node
    ret = rclc_node_init_default(&node, "stm32_main_node", "", &support);
    if (ret != RCL_RET_OK) return -1;

    // 5. Create Publisher
    ret = rclc_publisher_init_best_effort(
        &publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt8MultiArray),
        "deos_tx"
    );

    // 6. Create Subscriber
    sub_msg.data.data = sub_msg_buffer;
    sub_msg.data.capacity = sizeof(sub_msg_buffer);
    sub_msg.data.size = 0;
    
    ret = rclc_subscription_init_best_effort(
        &subscriber,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt8MultiArray),
        "deos_rx"
    );

    // 7. Initialize Executor
    ret = rclc_executor_init(&executor, &support.context, 1, &allocator);
    ret = rclc_executor_add_subscription(&executor, &subscriber, &sub_msg, &subscription_callback, ON_NEW_DATA);

    // 8. Start Executor Thread
    k_thread_create(&uros_thread, uros_stack, K_THREAD_STACK_SIZEOF(uros_stack),
                    uros_executor_thread, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
    deos_router_register_transport(DEOS_TRANSPORT_ETH_UROS, tx_callback);
    
    LOG_INF("micro-ROS Ethernet interface initialized successfully!");
    return 0;
}
