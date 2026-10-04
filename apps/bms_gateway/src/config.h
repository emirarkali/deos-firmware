#ifndef CONFIG_H
#define CONFIG_H

#include <deos/deos_icd.h>
#include <zephyr/devicetree.h>

#define DALY_BOARD_AUX         0x01
#define DALY_BOARD_MAIN        0x02
#define DALY_HOST_ADDRESS      0x40

#define BMS_TIMEOUT_MS         3000   /* Offline threshold */
#define BMS_CACHE_FRESHNESS_MS 1000   /* Cache freshness */

#ifndef DEOS_CAN_NODE
#define DEOS_CAN_NODE DT_ALIAS(deos_can)
#endif

#ifndef DALY_CAN_NODE
#define DALY_CAN_NODE DT_ALIAS(daly_can)
#endif

#endif /* CONFIG_H */
