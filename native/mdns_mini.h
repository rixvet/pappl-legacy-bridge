/*
 * mdns_mini.h — minimal mDNS/DNS-SD advertiser (RFC 6762).
 * See mdns_mini.c for details. No external dependencies.
 */
#ifndef MDNS_MINI_H
#define MDNS_MINI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mdns_handle mdns_handle_t;

/* Start advertising <service_name>._ipp._tcp.local on the given port.
 * rp = resource path for the TXT record, e.g. "ipp/print/Phaser6125".
 * Returns NULL on failure. */
mdns_handle_t *mdns_start(const char *service_name, uint16_t port, const char *rp);

/* Stop advertising and free resources. */
void mdns_stop(mdns_handle_t *h);

#ifdef __cplusplus
}
#endif

#endif /* MDNS_MINI_H */
