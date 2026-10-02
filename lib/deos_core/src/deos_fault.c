#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <errno.h>

LOG_MODULE_REGISTER(deos_fault, LOG_LEVEL_INF);



/*
 * Fault Transfer Context for GET_FAULTS
 */
struct deos_fault_transfer {
    bool active;
    deos_node_id_t destination;
    size_t current_index;
    struct k_work_delayable work;
};

struct deos_fault_context {
    deos_node_id_t node_id;
    deos_fault_record_t fault_table[DEOS_MAX_FAULT_RECORDS];
    struct k_mutex mutex;
    struct deos_fault_transfer transfer_ctx;
};

static struct deos_fault_context fault_contexts[DEOS_MAX_LOCAL_NODES];

static struct deos_fault_context *get_fault_context(deos_node_id_t node_id)
{
    int idx = deos_get_local_node_index(node_id);
    if (idx < 0) {
        return NULL;
    }
    
    /* Ensure the context knows its node ID */
    fault_contexts[idx].node_id = node_id;
    return &fault_contexts[idx];
}

static void send_fault_response(const deos_fault_record_t *record, deos_node_id_t source_node, deos_node_id_t destination)
{
    uint8_t payload[10] = {0};

    deos_put_u16_le(&payload[0], record->fault_id);
    payload[2] = record->severity;
    payload[3] = record->state;
    deos_put_u16_le(&payload[4], record->occurrence_count);
    deos_put_u32_le(&payload[6], record->last_occurrence_ms);

    int ret = deos_send_from_node(
        source_node,
        destination,
        DEOS_PRIO_STATUS,
        DEOS_CLASS_RESPONSE,
        DEOS_SERVICE_DIAGNOSTIC,
        DEOS_CMD_DIAG_GET_FAULTS,
        payload,
        10
    );

    if (ret != 0) {
        LOG_WRN("Failed to send fault response from 0x%02X to 0x%02X: %d", source_node, destination, ret);
    }
}

static void fault_transfer_work_handler(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct deos_fault_transfer *tctx = CONTAINER_OF(dwork, struct deos_fault_transfer, work);
    struct deos_fault_context *ctx = CONTAINER_OF(tctx, struct deos_fault_context, transfer_ctx);

    k_mutex_lock(&ctx->mutex, K_FOREVER);

    if (!tctx->active) {
        k_mutex_unlock(&ctx->mutex);
        return;
    }

    /* Find next used entry */
    bool found = false;
    deos_fault_record_t record;

    while (tctx->current_index < DEOS_MAX_FAULT_RECORDS) {
        if (ctx->fault_table[tctx->current_index].fault_id != DEOS_FAULT_ID_END_OF_LIST) {
            record = ctx->fault_table[tctx->current_index];
            tctx->current_index++;
            found = true;
            break;
        }
        tctx->current_index++;
    }

    k_mutex_unlock(&ctx->mutex);

    if (found) {
        send_fault_response(&record, ctx->node_id, tctx->destination);
        k_work_schedule(&tctx->work, K_MSEC(100));
    } else {
        /* End of list */
        deos_fault_record_t end_record = {
            .fault_id = DEOS_FAULT_ID_END_OF_LIST,
            .severity = 0,
            .state = 0,
            .occurrence_count = 0,
            .last_occurrence_ms = 0
        };
        send_fault_response(&end_record, ctx->node_id, tctx->destination);
        
        k_mutex_lock(&ctx->mutex, K_FOREVER);
        tctx->active = false;
        k_mutex_unlock(&ctx->mutex);
    }
}

int deos_fault_init(void)
{
    for (int i = 0; i < DEOS_MAX_LOCAL_NODES; i++) {
        k_mutex_init(&fault_contexts[i].mutex);
        for (int j = 0; j < DEOS_MAX_FAULT_RECORDS; j++) {
            fault_contexts[i].fault_table[j].fault_id = DEOS_FAULT_ID_END_OF_LIST;
        }
        fault_contexts[i].transfer_ctx.active = false;
        k_work_init_delayable(&fault_contexts[i].transfer_ctx.work, fault_transfer_work_handler);
    }

    LOG_DBG("Fault management initialized for multiple nodes");
    return 0;
}

int deos_fault_raise_for_node(deos_node_id_t local_node, uint16_t fault_id, deos_fault_severity_t severity)
{
    if (fault_id == DEOS_FAULT_ID_END_OF_LIST) {
        return -EINVAL;
    }

    struct deos_fault_context *ctx = get_fault_context(local_node);
    if (!ctx) {
        return -EPERM;
    }

    k_mutex_lock(&ctx->mutex, K_FOREVER);

    int free_slot = -1;
    bool found = false;
    uint32_t now = k_uptime_get_32();

    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        if (ctx->fault_table[i].fault_id == fault_id) {
            ctx->fault_table[i].state = DEOS_FAULT_STATE_ACTIVE;
            ctx->fault_table[i].severity = severity;
            if (ctx->fault_table[i].occurrence_count < UINT16_MAX) {
                ctx->fault_table[i].occurrence_count++;
            }
            ctx->fault_table[i].last_occurrence_ms = now;
            found = true;
            break;
        } else if (ctx->fault_table[i].fault_id == DEOS_FAULT_ID_END_OF_LIST && free_slot == -1) {
            free_slot = i;
        }
    }

    if (!found) {
        if (free_slot == -1) {
            k_mutex_unlock(&ctx->mutex);
            return -ENOSPC;
        }
        ctx->fault_table[free_slot].fault_id = fault_id;
        ctx->fault_table[free_slot].state = DEOS_FAULT_STATE_ACTIVE;
        ctx->fault_table[free_slot].severity = severity;
        ctx->fault_table[free_slot].occurrence_count = 1;
        ctx->fault_table[free_slot].last_occurrence_ms = now;
    }

    k_mutex_unlock(&ctx->mutex);
    LOG_WRN("[Node 0x%02X] Fault Raised: 0x%04X (Sev: %d)", local_node, fault_id, severity);
    return 0;
}

int deos_fault_set_inactive_for_node(deos_node_id_t local_node, uint16_t fault_id)
{
    if (fault_id == DEOS_FAULT_ID_END_OF_LIST) {
        return -EINVAL;
    }

    struct deos_fault_context *ctx = get_fault_context(local_node);
    if (!ctx) return -EPERM;

    int ret = -ENOENT;
    k_mutex_lock(&ctx->mutex, K_FOREVER);

    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        if (ctx->fault_table[i].fault_id == fault_id) {
            if (ctx->fault_table[i].state != DEOS_FAULT_STATE_LATCHED) {
                ctx->fault_table[i].state = DEOS_FAULT_STATE_INACTIVE;
            }
            ret = 0;
            break;
        }
    }

    k_mutex_unlock(&ctx->mutex);
    return ret;
}

int deos_fault_latch_for_node(deos_node_id_t local_node, uint16_t fault_id)
{
    if (fault_id == DEOS_FAULT_ID_END_OF_LIST) {
        return -EINVAL;
    }

    struct deos_fault_context *ctx = get_fault_context(local_node);
    if (!ctx) return -EPERM;

    int ret = -ENOENT;
    k_mutex_lock(&ctx->mutex, K_FOREVER);

    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        if (ctx->fault_table[i].fault_id == fault_id) {
            ctx->fault_table[i].state = DEOS_FAULT_STATE_LATCHED;
            ret = 0;
            break;
        }
    }

    k_mutex_unlock(&ctx->mutex);
    return ret;
}

int deos_fault_clear_for_node(deos_node_id_t local_node, uint16_t fault_id)
{
    if (fault_id == DEOS_FAULT_ID_END_OF_LIST) {
        return -EINVAL;
    }

    struct deos_fault_context *ctx = get_fault_context(local_node);
    if (!ctx) return -EPERM;

    int ret = -ENOENT;
    k_mutex_lock(&ctx->mutex, K_FOREVER);

    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        if (ctx->fault_table[i].fault_id == fault_id) {
            ctx->fault_table[i].fault_id = DEOS_FAULT_ID_END_OF_LIST;
            ret = 0;
            break;
        }
    }

    k_mutex_unlock(&ctx->mutex);
    return ret;
}

int deos_fault_clear_all_for_node(deos_node_id_t local_node)
{
    struct deos_fault_context *ctx = get_fault_context(local_node);
    if (!ctx) return -EPERM;

    k_mutex_lock(&ctx->mutex, K_FOREVER);
    
    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        ctx->fault_table[i].fault_id = DEOS_FAULT_ID_END_OF_LIST;
    }

    k_mutex_unlock(&ctx->mutex);
    LOG_INF("[Node 0x%02X] All faults cleared", local_node);
    return 0;
}

bool deos_fault_is_active_for_node(deos_node_id_t local_node, uint16_t fault_id)
{
    if (fault_id == DEOS_FAULT_ID_END_OF_LIST) {
        return false;
    }

    struct deos_fault_context *ctx = get_fault_context(local_node);
    if (!ctx) return false;

    bool active = false;
    k_mutex_lock(&ctx->mutex, K_FOREVER);

    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        if (ctx->fault_table[i].fault_id == fault_id) {
            if (ctx->fault_table[i].state == DEOS_FAULT_STATE_ACTIVE ||
                ctx->fault_table[i].state == DEOS_FAULT_STATE_LATCHED) {
                active = true;
            }
            break;
        }
    }

    k_mutex_unlock(&ctx->mutex);
    return active;
}

/* 
 * Primary node wrappers 
 */
int deos_fault_raise(uint16_t fault_id, deos_fault_severity_t severity)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;
    return deos_fault_raise_for_node(config->node_id, fault_id, severity);
}

int deos_fault_set_inactive(uint16_t fault_id)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;
    return deos_fault_set_inactive_for_node(config->node_id, fault_id);
}

int deos_fault_latch(uint16_t fault_id)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;
    return deos_fault_latch_for_node(config->node_id, fault_id);
}

int deos_fault_clear(uint16_t fault_id)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;
    return deos_fault_clear_for_node(config->node_id, fault_id);
}

int deos_fault_clear_all(void)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;
    return deos_fault_clear_all_for_node(config->node_id);
}

bool deos_fault_is_active(uint16_t fault_id)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return false;
    return deos_fault_is_active_for_node(config->node_id, fault_id);
}

/*
 * Dispatch Handlers
 */
void deos_fault_handle_get_faults(const deos_message_t *msg)
{
    struct deos_fault_context *ctx = get_fault_context(msg->destination);
    if (!ctx) return;

    k_mutex_lock(&ctx->mutex, K_FOREVER);

    if (ctx->transfer_ctx.active) {
        LOG_WRN("[Node 0x%02X] GET_FAULTS requested but transfer already active. Ignoring.", msg->destination);
        k_mutex_unlock(&ctx->mutex);
        return;
    }

    ctx->transfer_ctx.active = true;
    ctx->transfer_ctx.destination = msg->source;
    ctx->transfer_ctx.current_index = 0;

    k_work_schedule(&ctx->transfer_ctx.work, K_NO_WAIT);

    k_mutex_unlock(&ctx->mutex);
}

void deos_fault_handle_clear_faults(const deos_message_t *msg)
{
    LOG_INF("[Node 0x%02X] CLEAR_FAULTS requested from 0x%02X", msg->destination, msg->source);
    deos_fault_clear_all_for_node(msg->destination);
}
