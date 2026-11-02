/*
 * snmp_poll.h — minimal SNMPv1 poller for the Phaser 6125N status bridge.
 *
 * Reads the Printer-MIB supplies table and alert table over UDP/161 and
 * maps them to plain C structs.  No external SNMP library — just BER
 * encode/decode for GET/GETNEXT (RFC 1157 subset).
 */
#ifndef SNMP_POLL_H
#define SNMP_POLL_H

#include <stdint.h>
#include <stdbool.h>

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wpadded"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SNMP_MAX_SUPPLIES  16
#define SNMP_MAX_ALERTS    16

typedef struct {
    char description[128];
    int  type;
    int  color;
    int  max_capacity;
    int  level_raw;
    int  level_pct;          /* 0-100, or -1 if unknown */
    bool is_consumed;
} snmp_supply_t;

typedef struct {
    int  severity;
    int  group;
    int  code;
    char text[256];
} snmp_alert_t;

typedef struct {
    bool online;
    int  device_status;      /* hrDeviceStatus: 3=running 6=down */
    int  num_supplies;
    snmp_supply_t supplies[SNMP_MAX_SUPPLIES];
    int  num_alerts;
    snmp_alert_t alerts[SNMP_MAX_ALERTS];
} snmp_status_t;

/* Poll printer at host (UDP/161) with community. 0 = ok, -1 = offline. */
int snmp_poll_printer(const char *host, const char *community, snmp_status_t *st);
void snmp_status_free(snmp_status_t *st);

#ifdef __cplusplus
}
#endif

#endif /* SNMP_POLL_H */
