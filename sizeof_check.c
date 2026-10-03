#include <stdio.h>
#include <stdint.h>

typedef enum { A = 0, B } deos_priority_t;
typedef enum { C = 0, D } deos_message_class_t;
typedef enum { E = 0, F } deos_service_id_t;
typedef enum { G = 0, H } deos_node_id_t;

#define DEOS_MAX_PAYLOAD_LEN 60

typedef struct {
    deos_priority_t priority;
    deos_message_class_t message_class;
    deos_service_id_t service;
    deos_node_id_t destination;
    deos_node_id_t source;
    uint8_t version;
    uint8_t sequence;
    uint8_t command;
    uint8_t payload[DEOS_MAX_PAYLOAD_LEN];
    uint8_t payload_len;
} deos_message_t;

int main() {
    printf("Size: %zu\n", sizeof(deos_message_t));
    return 0;
}
