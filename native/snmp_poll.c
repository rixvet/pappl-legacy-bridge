/*
 * snmp_poll.c — minimal SNMPv1 poller (BER subset) for Printer-MIB.
 *
 * Strategy: 1 GET for sysDescr (online check), 1 GET for hrDeviceStatus,
 * a short GETNEXT walk to count supply rows, then chunked multi-OID GETs
 * (batch=10 — the printer's SNMP buffer chokes on >~20 varbinds) for all
 * supply columns and alert columns.
 */
#include "snmp_poll.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wpadded"
#  pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#  pragma clang diagnostic ignored "-Wshadow"
#endif

#ifndef _WIN32
#  include <unistd.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <sys/time.h>
#else
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
#endif

/* ---- BER encode ---- */

static int ber_tlv(uint8_t *buf, int maxlen, int pos, uint8_t tag,
                   const uint8_t *val, int vlen)
{
    if (pos + 2 + vlen > maxlen) return -1;
    buf[pos++] = tag;
    if (vlen < 128) {
        buf[pos++] = (uint8_t)vlen;
    } else if (vlen < 256) {
        if (pos + 3 + vlen > maxlen) return -1;
        buf[pos++] = 0x81;
        buf[pos++] = (uint8_t)vlen;
    } else {
        if (pos + 4 + vlen > maxlen) return -1;
        buf[pos++] = 0x82;
        buf[pos++] = (uint8_t)(vlen >> 8);
        buf[pos++] = (uint8_t)(vlen & 0xFF);
    }
    memcpy(buf + pos, val, (size_t)vlen);
    return pos + vlen;
}

static int ber_int(uint8_t *buf, int maxlen, int pos, long v)
{
    uint8_t tmp[8];
    int n = 0;
    if (v == 0) {
        tmp[n++] = 0;
    } else {
        unsigned long u = (unsigned long)v;
        while (u) { tmp[n++] = (uint8_t)(u & 0xFF); u >>= 8; }
        if (v < 0) {
            int i;
            for (i = 0; i < n; i++) tmp[i] = (uint8_t)~tmp[i];
            for (i = 0; i < n; i++) { tmp[i]++; if (tmp[i]) break; }
        } else if (tmp[n - 1] & 0x80) {
            tmp[n++] = 0;
        }
        int i, j;
        for (i = 0, j = n - 1; i < j; i++, j--) { uint8_t t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t; }
    }
    return ber_tlv(buf, maxlen, pos, 0x02, tmp, n);
}

static int ber_oid(uint8_t *buf, int maxlen, int pos, const int *oid, int oidlen)
{
    uint8_t tmp[256];
    int n = 0;
    if (oidlen < 2) return -1;
    tmp[n++] = (uint8_t)(oid[0] * 40 + oid[1]);
    for (int i = 2; i < oidlen; i++) {
        long v = oid[i];
        if (v == 0) {
            if (n >= (int)sizeof tmp) return -1;
            tmp[n++] = 0;
        } else {
            uint8_t part[8];
            int pn = 0;
            unsigned long u = (unsigned long)v;
            while (u) { part[pn++] = (uint8_t)(u & 0x7F); u >>= 7; }
            for (int a = pn - 1; a >= 0; a--) {
                if (n >= (int)sizeof tmp) return -1;
                tmp[n++] = (uint8_t)(part[a] | (a ? 0x80 : 0));
            }
        }
    }
    return ber_tlv(buf, maxlen, pos, 0x06, tmp, n);
}

static int ber_str(uint8_t *buf, int maxlen, int pos, const char *s)
{
    return ber_tlv(buf, maxlen, pos, 0x04, (const uint8_t *)s, (int)strlen(s));
}

/* Build GET/GETNEXT with multiple OIDs. oids is flat: 32 ints per OID. */
static int snmp_build_get_multi(uint8_t *buf, int maxlen, int reqid,
                                const char *community,
                                const int *oids, const int *oid_lens, int num_oids,
                                bool getnext)
{
    uint8_t pdu[1400];
    int p = 0;
    p = ber_int(pdu, sizeof pdu, p, reqid);
    p = ber_int(pdu, sizeof pdu, p, 0);
    p = ber_int(pdu, sizeof pdu, p, 0);
    uint8_t vbl[1200];
    int vp = 0;
    for (int i = 0; i < num_oids; i++) {
        uint8_t vb[64];
        int q = 0;
        q = ber_oid(vb, sizeof vb, q, oids + i * 32, oid_lens[i]);
        uint8_t nullv = 0;
        q = ber_tlv(vb, sizeof vb, q, 0x05, &nullv, 0);
        vp = ber_tlv(vbl, sizeof vbl, vp, 0x30, vb, q);
    }
    p = ber_tlv(pdu, sizeof pdu, p, 0x30, vbl, vp);
    uint8_t pdu_full[1500];
    int pp = 0;
    pp = ber_tlv(pdu_full, sizeof pdu_full, pp, getnext ? 0xA1 : 0xA0, pdu, p);
    uint8_t content[1600];
    int c = 0;
    c = ber_int(content, sizeof content, c, 0);
    c = ber_str(content, sizeof content, c, community);
    memcpy(content + c, pdu_full, (size_t)pp); c += pp;
    return ber_tlv(buf, maxlen, 0, 0x30, content, c);
}

static int snmp_build_get(uint8_t *buf, int maxlen, int reqid,
                          const char *community, const int *oid, int oidlen, bool getnext)
{
    return snmp_build_get_multi(buf, maxlen, reqid, community, oid, &oidlen, 1, getnext);
}

/* ---- BER decode ---- */

static int ber_read_tlv(const uint8_t *buf, int len, int pos, uint8_t *tag, int *vlen, int *vpos)
{
    if (pos >= len) return -1;
    *tag = buf[pos++];
    if (pos >= len) return -1;
    int l = buf[pos++];
    if (l & 0x80) {
        int nb = l & 0x7F;
        if (nb > 4 || pos + nb > len) return -1;
        l = 0;
        for (int i = 0; i < nb; i++) l = (l << 8) | buf[pos++];
    }
    if (pos + l > len) return -1;
    *vlen = l;
    *vpos = pos;
    return pos + l;
}

static int ber_parse_oid(const uint8_t *buf, int vlen, int vpos, int *oid, int max_oid)
{
    int n = 0;
    long v = 0;
    int first = 1;
    for (int i = vpos; i < vpos + vlen; i++) {
        uint8_t b = buf[i];
        v = (v << 7) | (b & 0x7F);
        if (!(b & 0x80)) {
            if (first) {
                if (n < max_oid) oid[n++] = (int)(v / 40);
                if (n < max_oid) oid[n++] = (int)(v % 40);
                first = 0;
            } else {
                if (n < max_oid) oid[n++] = (int)v;
            }
            v = 0;
        }
    }
    return n;
}

static long ber_parse_int(const uint8_t *buf, int vlen, int vpos)
{
    long v = 0;
    for (int i = vpos; i < vpos + vlen; i++)
        v = (v << 8) | buf[i];
    if (vlen > 0 && vlen <= 8) {
        int shift = (8 - vlen) * 8;
        v = (v << shift) >> shift;
    }
    return v;
}

static int snmp_parse_response_multi(const uint8_t *buf, int len, int reqid,
                                     int *oid_out, int *oid_lens, uint8_t *types,
                                     const uint8_t **vals, int *vlens,
                                     int max_vars, int *num_out)
{
    uint8_t tag;
    int vlen, vpos;
    int pos = ber_read_tlv(buf, len, 0, &tag, &vlen, &vpos);
    if (pos < 0 || tag != 0x30) return -1;
    pos = ber_read_tlv(buf, len, vpos, &tag, &vlen, &vpos);
    if (tag != 0x02) return -1;
    pos = ber_read_tlv(buf, len, pos, &tag, &vlen, &vpos);
    if (tag != 0x04) return -1;
    pos = ber_read_tlv(buf, len, pos, &tag, &vlen, &vpos);
    if (tag != 0xA2) return -1;
    int pdu_end = vpos + vlen;
    int rpos = ber_read_tlv(buf, pdu_end, vpos, &tag, &vlen, &vpos);
    if (tag != 0x02 || ber_parse_int(buf, vlen, vpos) != reqid) return -1;
    rpos = ber_read_tlv(buf, pdu_end, rpos, &tag, &vlen, &vpos);
    if (ber_parse_int(buf, vlen, vpos) != 0) return -1;
    rpos = ber_read_tlv(buf, pdu_end, rpos, &tag, &vlen, &vpos);
    ber_read_tlv(buf, pdu_end, rpos, &tag, &vlen, &vpos);
    if (tag != 0x30) return -1;
    int vb_end = vpos + vlen;
    int n = 0;
    int vbpos = vpos;
    while (vbpos < vb_end && n < max_vars) {
        int vend;
        int q = ber_read_tlv(buf, vb_end, vbpos, &tag, &vlen, &vpos);
        if (q < 0 || tag != 0x30) break;
        vend = vpos + vlen;
        int opos = ber_read_tlv(buf, vend, vpos, &tag, &vlen, &vpos);
        if (tag != 0x06) break;
        oid_lens[n] = ber_parse_oid(buf, vlen, vpos, oid_out + n * 32, 32);
        int valpos = ber_read_tlv(buf, vend, opos, &tag, &vlen, &vpos);
        if (valpos < 0) break;
        types[n] = tag;
        vals[n] = buf + vpos;
        vlens[n] = vlen;
        n++;
        vbpos = vend;
    }
    *num_out = n;
    return 0;
}

static int snmp_parse_response(const uint8_t *buf, int len, int reqid,
                               int *oid_out, int *oid_len, uint8_t *type_out,
                               const uint8_t **val_out, int *val_len)
{
    int oids[32];
    int olens[1];
    uint8_t types[1];
    const uint8_t *vals[1];
    int vlens[1];
    int total = 0;
    int rc = snmp_parse_response_multi(buf, len, reqid, oids, olens, types, vals, vlens, 1, &total);
    if (rc == 0 && total > 0) {
        memcpy(oid_out, oids, (size_t)(olens[0] * sizeof(int)));
        *oid_len = olens[0];
        *type_out = types[0];
        *val_out = vals[0];
        *val_len = vlens[0];
    }
    return rc;
}

/* ---- UDP transport ---- */

static int snmp_udp_exchange(const char *host, uint16_t port,
                             const uint8_t *req, int reqlen,
                             uint8_t *resp, int resplen, int timeout_ms)
{
#ifndef _WIN32
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) return -1;
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    struct sockaddr_in dst;
    memset(&dst, 0, sizeof dst);
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    inet_pton(AF_INET, host, &dst.sin_addr);
    if (sendto(s, (const char *)req, (size_t)reqlen, 0, (struct sockaddr *)&dst, sizeof dst) < 0) {
        close(s); return -1;
    }
    struct sockaddr_in from;
    socklen_t fl = sizeof from;
    ssize_t n = recvfrom(s, (char *)resp, (size_t)resplen, 0, (struct sockaddr *)&from, &fl);
    close(s);
    return (int)n;
#else
    static int wsa_init = 0;
    if (!wsa_init) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); wsa_init = 1; }
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return -1;
    DWORD tv = (DWORD)timeout_ms;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
    struct sockaddr_in dst;
    memset(&dst, 0, sizeof dst);
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    inet_pton(AF_INET, host, &dst.sin_addr);
    if (sendto(s, (const char *)req, reqlen, 0, (struct sockaddr *)&dst, sizeof dst) < 0) {
        closesocket(s); return -1;
    }
    struct sockaddr_in from;
    int fl = sizeof from;
    int n = recvfrom(s, (char *)resp, resplen, 0, (struct sockaddr *)&from, &fl);
    closesocket(s);
    return n;
#endif
}

/* ---- Row collectors ---- */

typedef struct {
    snmp_status_t *st;
} walk_ctx_t;

static void supplies_cb(const int *oid, int oidlen, uint8_t type,
                        const uint8_t *val, int vlen, void *ctx_)
{
    walk_ctx_t *ctx = (walk_ctx_t *)ctx_;
    snmp_status_t *st = ctx->st;
    if (oidlen < 3) return;
    int col = oid[oidlen - 3];
    int idx = oid[oidlen - 1];
    if (idx < 1 || idx > SNMP_MAX_SUPPLIES) return;
    if (st->num_supplies < idx) st->num_supplies = idx;
    snmp_supply_t *s = &st->supplies[idx - 1];
    switch (col) {
    case 3:
        if (type == 0x02) s->color = (int)ber_parse_int(val, vlen, 0);
        break;
    case 6:
        if (type == 0x04) {
            int n = vlen < (int)sizeof(s->description) - 1 ? vlen : (int)sizeof(s->description) - 1;
            memcpy(s->description, val, (size_t)n);
            s->description[n] = 0;
        }
        break;
    case 7:
        if (type == 0x02) s->type = (int)ber_parse_int(val, vlen, 0);
        break;
    case 8:
        if (type == 0x02) s->max_capacity = (int)ber_parse_int(val, vlen, 0);
        break;
    case 9:
        if (type == 0x02) {
            s->level_raw = (int)ber_parse_int(val, vlen, 0);
            if (s->level_raw < 0) {
                s->level_pct = -1;
            } else if (s->max_capacity > 0) {
                s->level_pct = (int)((long)s->level_raw * 100 / s->max_capacity);
                if (s->level_pct > 100) s->level_pct = 100;
            } else {
                s->level_pct = s->level_raw > 100 ? 100 : s->level_raw;
            }
        }
        break;
    }
    s->is_consumed = true;
}

static void alerts_cb(const int *oid, int oidlen, uint8_t type,
                      const uint8_t *val, int vlen, void *ctx_)
{
    snmp_status_t *st = (snmp_status_t *)ctx_;
    if (oidlen < 3) return;
    int col = oid[oidlen - 3];  /* alerts: 43.18.1.1.<col>.<grp>.<idx> */
    int idx = oid[oidlen - 1];
    if (st->num_alerts < idx) st->num_alerts = idx;
    if (idx < 1 || idx > SNMP_MAX_ALERTS) return;
    snmp_alert_t *a = &st->alerts[idx - 1];
    switch (col) {
    case 2: if (type == 0x02) a->severity = (int)ber_parse_int(val, vlen, 0); break;
    case 3: if (type == 0x02) a->group = (int)ber_parse_int(val, vlen, 0); break;
    case 4: if (type == 0x02) a->code = (int)ber_parse_int(val, vlen, 0); break;
    case 8:
        if (type == 0x04) {
            int n = vlen < (int)sizeof(a->text) - 1 ? vlen : (int)sizeof(a->text) - 1;
            memcpy(a->text, val, (size_t)n);
            a->text[n] = 0;
        }
        break;
    }
}

/* ---- Public API ---- */

int snmp_poll_printer(const char *host, const char *community, snmp_status_t *st)
{
    memset(st, 0, sizeof *st);
    st->online = false;

    /* sysDescr probe */
    {
        static const int sysdescr[] = { 1,3,6,1,2,1,1,1,0 };
        uint8_t req[256], resp[1024];
        int rl = snmp_build_get(req, sizeof req, 1, community, sysdescr, 9, false);
        int rn = snmp_udp_exchange(host, 161, req, rl, resp, sizeof resp, 10000);
        if (rn <= 0) return -1;
        int roid[32], roidlen;
        uint8_t rtype; const uint8_t *rval; int rvlen;
        if (snmp_parse_response(resp, rn, 1, roid, &roidlen, &rtype, &rval, &rvlen) != 0)
            return -1;
    }
    st->online = true;

    /* hrDeviceStatus */
    {
        static const int hrdev[] = { 1,3,6,1,2,1,25,3,5,1,1,1 };
        uint8_t req[256], resp[1024];
        int rl = snmp_build_get(req, sizeof req, 2, community, hrdev, 12, false);
        int rn = snmp_udp_exchange(host, 161, req, rl, resp, sizeof resp, 10000);
        if (rn > 0) {
            int roid[32], roidlen;
            uint8_t rtype; const uint8_t *rval; int rvlen;
            if (snmp_parse_response(resp, rn, 2, roid, &roidlen, &rtype, &rval, &rvlen) == 0 && rtype == 0x02)
                st->device_status = (int)ber_parse_int(rval, rvlen, 0);
        }
    }

    /* Count supply rows via short GETNEXT walk on level column */
    int num_supply_rows = 0;
    {
        static const int lvl_base[] = { 1,3,6,1,2,1,43,11,1,1,9 };
        uint8_t req[256], resp[1024];
        int oid[32], oidlen = 11;
        memcpy(oid, lvl_base, sizeof lvl_base);
        for (int i = 0; i < 16; i++) {
            int rl = snmp_build_get(req, sizeof req, 10 + i, community, oid, oidlen, true);
            int rn = snmp_udp_exchange(host, 161, req, rl, resp, sizeof resp, 10000);
            if (rn <= 0) break;
            int roid[32], roidlen;
            uint8_t rtype; const uint8_t *rval; int rvlen;
            if (snmp_parse_response(resp, rn, 10 + i, roid, &roidlen, &rtype, &rval, &rvlen) != 0) break;
            bool in_sub = (roidlen >= 12 && roid[6]==43 && roid[7]==11 && roid[8]==1 && roid[9]==1 && roid[10]==9);
            if (!in_sub) break;
            num_supply_rows = roid[roidlen - 1];
            memcpy(oid, roid, (size_t)(roidlen * sizeof(int)));
            oidlen = roidlen;
        }
    }
    if (num_supply_rows > SNMP_MAX_SUPPLIES) num_supply_rows = SNMP_MAX_SUPPLIES;

    /* Chunked multi-GET for supply columns × rows (batch=10) */
    if (num_supply_rows > 0) {
        int oids[32 * 80];
        int oid_lens[80];
        int n = 0;
        static const int cols[] = { 3, 6, 7, 8, 9 };
        static const int sbase[] = { 1,3,6,1,2,1,43,11,1,1 };
        for (unsigned ci = 0; ci < sizeof(cols)/sizeof(cols[0]); ci++) {
            for (int row = 1; row <= num_supply_rows; row++) {
                int *o = oids + n * 32;
                memcpy(o, sbase, sizeof sbase);
                o[10] = cols[ci];
                o[11] = 1;
                o[12] = row;
                oid_lens[n] = 13;
                n++;
            }
        }
        for (int chunk = 0; chunk < n; chunk += 4) {
            int cnt = n - chunk;
            if (cnt > 4) cnt = 4;
            uint8_t req[1600], resp[4096];
            int rl = snmp_build_get_multi(req, sizeof req, 100 + chunk, community,
                                          oids + chunk * 32, oid_lens + chunk, cnt, false);
            int rn = snmp_udp_exchange(host, 161, req, rl, resp, sizeof resp, 10000);
            if (rn <= 0) continue;
            int roids[32 * 8]; int roid_lens[8]; uint8_t rtypes[8];
            const uint8_t *rvals[8]; int rvlens[8];
            int num = 0;
            if (snmp_parse_response_multi(resp, rn, 100 + chunk, roids, roid_lens, rtypes, rvals, rvlens, 8, &num) == 0) {
                for (int i = 0; i < num; i++) {
                    walk_ctx_t wctx;
                    wctx.st = st;
                    supplies_cb(roids + i * 32, roid_lens[i], rtypes[i], rvals[i], rvlens[i], &wctx);
                }
            }
        }
    }
    st->num_supplies = num_supply_rows;

    /* Alerts: count rows, then chunked multi-GET */
    {
        int num_alert_rows = 0;
        static const int sev_base[] = { 1,3,6,1,2,1,43,18,1,1,2 };
        uint8_t req[256], resp[1024];
        int oid[32], oidlen = 11;
        memcpy(oid, sev_base, sizeof sev_base);
        for (int i = 0; i < 16; i++) {
            int rl = snmp_build_get(req, sizeof req, 210 + i, community, oid, oidlen, true);
            int rn = snmp_udp_exchange(host, 161, req, rl, resp, sizeof resp, 10000);
            if (rn <= 0) break;
            int roid[32], roidlen;
            uint8_t rtype; const uint8_t *rval; int rvlen;
            if (snmp_parse_response(resp, rn, 210 + i, roid, &roidlen, &rtype, &rval, &rvlen) != 0) break;
            bool in_sub = (roidlen >= 11 && roid[6]==43 && roid[7]==18 && roid[8]==1 && roid[9]==1 && roid[10]==2);
            if (!in_sub) break;
            num_alert_rows = roid[roidlen - 1];
            memcpy(oid, roid, (size_t)(roidlen * sizeof(int)));
            oidlen = roidlen;
        }
        if (num_alert_rows > SNMP_MAX_ALERTS) num_alert_rows = SNMP_MAX_ALERTS;
        if (num_alert_rows > 0) {
            int oids[32 * 32]; int oid_lens[32]; int n = 0;
            static const int cols[] = { 2, 3, 4, 8 };
            static const int abase[] = { 1,3,6,1,2,1,43,18,1,1 };
            for (unsigned ci = 0; ci < sizeof(cols)/sizeof(cols[0]); ci++) {
                for (int row = 1; row <= num_alert_rows; row++) {
                    int *o = oids + n * 32;
                    memcpy(o, abase, sizeof abase);
                    o[10] = cols[ci];
                    o[11] = 1;      /* group index (same as supplies) */
                    o[12] = row;
                    oid_lens[n] = 13;
                    n++;
                }
            }
            for (int chunk = 0; chunk < n; chunk += 4) {
                int cnt = n - chunk;
                if (cnt > 4) cnt = 4;
                uint8_t areq[1200], aresp[2048];
                int arl = snmp_build_get_multi(areq, sizeof areq, 300 + chunk, community,
                                              oids + chunk * 32, oid_lens + chunk, cnt, false);
                int arn = snmp_udp_exchange(host, 161, areq, arl, aresp, sizeof aresp, 10000);
                if (arn <= 0) continue;
                int roids[32 * 8]; int roid_lens[8]; uint8_t rtypes[8];
                const uint8_t *rvals[8]; int rvlens[8];
                int num = 0;
                if (snmp_parse_response_multi(aresp, arn, 300 + chunk, roids, roid_lens, rtypes, rvals, rvlens, 8, &num) == 0) {
                    for (int i = 0; i < num; i++)
                        alerts_cb(roids + i * 32, roid_lens[i], rtypes[i], rvals[i], rvlens[i], st);
                }
            }
        }
        st->num_alerts = num_alert_rows;
    }

    return 0;
}

void snmp_status_free(snmp_status_t *st)
{
    (void)st;
}
