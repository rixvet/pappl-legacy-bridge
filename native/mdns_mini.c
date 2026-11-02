/*
 * mdns_mini.c — minimal mDNS/DNS-SD advertiser (RFC 6762) for the
 * Phaser 6125N PAPPL app.  No Bonjour/Apple dnssd.dll needed.
 *
 * Advertises _ipp._tcp (and _universal._tcp subtype) so Windows 11
 * "Add printer" finds us automatically via multicast DNS.
 *
 * Usage:
 *   mdns_handle_t *h = mdns_start("Phaser6125", 8631, "ipp/print/Phaser6125");
 *   ...
 *   mdns_stop(h);
 *
 * Implementation: sends unsolicited announcements on start, then answers
 * PTR queries for _ipp._tcp.local / _universal._tcp.local with our
 * PTR+SRV+TXT+A record set.  Single UDP socket on 224.0.0.251:5353.
 */
#include "mdns_mini.h"

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wpadded"
#  pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <net/if.h>
#  include <pthread.h>
#  include <time.h>
#else
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  pragma comment(lib, "ws2_32.lib")
#endif

#ifdef _WIN32
/* mingw's ws2tcpip.h lacks InetPtonA on older SDKs; use inet_addr for our
 * IPv4 literal strings (224.0.0.251, 8.8.8.8, 127.0.0.1). */
static int win_inet_pton(const char *s, void *d)
{
    unsigned long a = (unsigned long)inet_addr(s);
    if (a == (unsigned long)INADDR_NONE) return 0;
    memcpy(d, &a, sizeof a);
    return 1;
}
#  define inet_pton(f, s, d) win_inet_pton(s, d)
#endif

#define MDNS_PORT     5353
#define MDNS_GROUP_V4 "224.0.0.251"
#define BUF_SIZE      9000

struct mdns_handle {
    char service_name[64];   /* e.g. "Phaser6125" */
    char host_name[64];      /* e.g. "phaser6125-host" */
    uint16_t port;
    char rp[128];            /* resource path, e.g. "ipp/print/Phaser6125" */
    char uuid[64];
    int sock;
    int running;
#ifndef _WIN32
    pthread_t thread;
#else
    HANDLE thread;
#endif
    uint32_t our_ip;         /* network byte order */
};

/* ---- DNS name encoding ---- */
static int put_name(uint8_t *buf, int maxlen, int pos, const char *name)
{
    /* Encode "a.b.c" as DNS labels */
    const char *p = name;
    while (*p) {
        const char *dot = strchr(p, '.');
        int len = dot ? (int)(dot - p) : (int)strlen(p);
        if (len > 63 || pos + 1 + len >= maxlen) return -1;
        buf[pos++] = (uint8_t)len;
        memcpy(buf + pos, p, (size_t)len);
        pos += len;
        if (!dot) break;
        p = dot + 1;
    }
    if (pos >= maxlen) return -1;
    buf[pos++] = 0;
    return pos;
}

/* Append a compressed-name pointer */
static int put_ptr(uint8_t *buf, int maxlen, int pos, int offset)
{
    if (pos + 2 > maxlen) return -1;
    buf[pos++] = 0xC0 | ((offset >> 8) & 0x3F);
    buf[pos++] = offset & 0xFF;
    return pos;
}

static void put_u16(uint8_t *buf, int pos, uint16_t v)
{
    buf[pos] = (uint8_t)(v >> 8);
    buf[pos + 1] = (uint8_t)(v & 0xFF);
}

static void put_u32(uint8_t *buf, int pos, uint32_t v)
{
    buf[pos] = (uint8_t)(v >> 24);
    buf[pos + 1] = (uint8_t)((v >> 16) & 0xFF);
    buf[pos + 2] = (uint8_t)((v >> 8) & 0xFF);
    buf[pos + 3] = (uint8_t)(v & 0xFF);
}

/* Build the full announcement packet. Returns length, or -1. */
static int build_announcement(mdns_handle_t *h, uint8_t *buf, int maxlen, int is_response)
{
    int pos = 12;  /* skip DNS header */
    int name_off[8];
    char fqdn_srv[160], fqdn_txt[160], fqdn_ptr_ipp[80], fqdn_ptr_univ[80];
    char host_fqdn[80];
    int i;

    /* Header: 12 bytes */
    if (maxlen < 12) return -1;
    memset(buf, 0, 12);
    put_u16(buf, 2, is_response ? 0x8400 : 0x0000);  /* flags at offset 2 */
    /* ID=0, QDCOUNT=0 (offset 4), ANCOUNT filled later (offset 6) */

    snprintf(fqdn_srv, sizeof fqdn_srv, "%s._ipp._tcp.local", h->service_name);
    snprintf(fqdn_txt, sizeof fqdn_txt, "%s._ipp._tcp.local", h->service_name);
    snprintf(fqdn_ptr_ipp, sizeof fqdn_ptr_ipp, "_ipp._tcp.local");
    snprintf(fqdn_ptr_univ, sizeof fqdn_ptr_univ, "_universal._tcp.local");
    snprintf(host_fqdn, sizeof host_fqdn, "%s.local", h->host_name);

    /* --- PTR _ipp._tcp.local → service --- */
    name_off[0] = pos;
    pos = put_name(buf, maxlen, pos, fqdn_ptr_ipp);
    if (pos < 0) return -1;
    name_off[1] = pos;
    pos = put_name(buf, maxlen, pos, fqdn_srv);
    if (pos < 0) return -1;
    /* PTR record: type=12(PTR) class=1 ttl=4500 rdlen=ptr-to-name */
    if (pos + 10 > maxlen) return -1;
    put_u16(buf, pos, 12); put_u16(buf, pos + 2, 0x8001); /* class IN + cache flush */
    put_u32(buf, pos + 4, 4500);
    int rdlen_pos = pos + 8;
    pos += 10;
    int rd_start = pos;
    pos = put_ptr(buf, maxlen, pos, name_off[1]);
    if (pos < 0) return -1;
    put_u16(buf, rdlen_pos, (uint16_t)(pos - rd_start));

    /* --- PTR _universal._tcp.local → service (subtype) --- */
    if (pos + 12 > maxlen) return -1;
    name_off[2] = pos;
    pos = put_name(buf, maxlen, pos, fqdn_ptr_univ);
    if (pos < 0) return -1;
    if (pos + 10 > maxlen) return -1;
    put_u16(buf, pos, 12); put_u16(buf, pos + 2, 0x8001);
    put_u32(buf, pos + 4, 4500);
    rdlen_pos = pos + 8;
    pos += 10;
    rd_start = pos;
    pos = put_ptr(buf, maxlen, pos, name_off[1]); /* same SRV name */
    if (pos < 0) return -1;
    put_u16(buf, rdlen_pos, (uint16_t)(pos - rd_start));

    /* --- SRV service._ipp._tcp.local → host:port --- */
    if (pos + 12 > maxlen) return -1;
    pos = put_ptr(buf, maxlen, pos, name_off[1]); /* reuse name */
    if (pos < 0) return -1;
    if (pos + 10 > maxlen) return -1;
    put_u16(buf, pos, 33); put_u16(buf, pos + 2, 0x8001); /* SRV */
    put_u32(buf, pos + 4, 120);
    rdlen_pos = pos + 8;
    pos += 10;
    rd_start = pos;
    if (pos + 6 > maxlen) return -1;
    put_u16(buf, pos, 0); put_u16(buf, pos + 2, 0); put_u16(buf, pos + 4, h->port);
    pos += 6;
    name_off[3] = pos;
    pos = put_name(buf, maxlen, pos, host_fqdn);
    if (pos < 0) return -1;
    put_u16(buf, rdlen_pos, (uint16_t)(pos - rd_start));

    /* --- TXT service._ipp._tcp.local --- */
    if (pos + 12 > maxlen) return -1;
    pos = put_ptr(buf, maxlen, pos, name_off[1]);
    if (pos < 0) return -1;
    if (pos + 10 > maxlen) return -1;
    put_u16(buf, pos, 16); put_u16(buf, pos + 2, 0x8001); /* TXT */
    put_u32(buf, pos + 4, 4500);
    rdlen_pos = pos + 8;
    pos += 10;
    rd_start = pos;
    {
        char txt[512];
        int tp = 0;
        const char *items[16];
        int n = 0;
        char rp_full[200];
        snprintf(rp_full, sizeof rp_full, "rp=%s", h->rp);
        items[n++] = "txtvers=1";
        items[n++] = "qtotal=1";
        items[n++] = rp_full;
        items[n++] = "adminurl=http://localhost:8631/";
        items[n++] = "note=Xerox Phaser 6125N";
        items[n++] = "PDL=application/pdf,image/jpeg,image/png,image/pwg-raster,application/vnd.xerox-hbpl";
        items[n++] = "UUID=db79605f-e2c4-34d2-736b-0dec0756a120";
        items[n++] = "TLS=none";
        items[n++] = "UUID=db79605f-e2c4-34d2-736b-0dec0756a120";
        items[n++] = "pdl=application/pdf,image/jpeg,image/png";
        items[n++] = "product=(Phaser 6125N)";
        items[n++] = "priority=0";
        for (i = 0; i < n; i++) {
            int l = (int)strlen(items[i]);
            if (tp + 1 + l >= (int)sizeof txt) break;
            txt[tp++] = (char)l;
            memcpy(txt + tp, items[i], (size_t)l);
            tp += l;
        }
        if (pos + tp > maxlen) return -1;
        memcpy(buf + pos, txt, (size_t)tp);
        pos += tp;
    }
    put_u16(buf, rdlen_pos, (uint16_t)(pos - rd_start));

    /* --- A host.local → our_ip --- */
    if (pos + 12 > maxlen) return -1;
    pos = put_ptr(buf, maxlen, pos, name_off[3]);
    if (pos < 0) return -1;
    if (pos + 10 > maxlen) return -1;
    put_u16(buf, pos, 1); put_u16(buf, pos + 2, 0x8001); /* A */
    put_u32(buf, pos + 4, 120);
    put_u16(buf, pos + 8, 4);
    pos += 10;
    put_u32(buf, pos, h->our_ip);
    pos += 4;

    /* ANCOUNT = 4 */
    put_u16(buf, 6, 4);  /* ANCOUNT */
    return pos;
}

#ifndef _WIN32
static void *mdns_thread(void *arg)
#else
static DWORD WINAPI mdns_thread(LPVOID arg)
#endif
{
    mdns_handle_t *h = (mdns_handle_t *)arg;
    uint8_t buf[BUF_SIZE];
    struct sockaddr_in group;

    memset(&group, 0, sizeof group);
    group.sin_family = AF_INET;
    group.sin_port = htons(MDNS_PORT);
    inet_pton(AF_INET, MDNS_GROUP_V4, &group.sin_addr);

    /* Send 3 announcements, 1 second apart (RFC 6762 §8.3) */
    for (int i = 0; i < 3 && h->running; i++) {
        int len = build_announcement(h, buf, sizeof buf, 0);
        if (len > 0)
            sendto(h->sock, (const char *)buf, (size_t)len, 0,
                   (struct sockaddr *)&group, sizeof group);
#ifndef _WIN32
        sleep(1);
#else
        Sleep(1000);
#endif
    }

    /* Answer queries */
    while (h->running) {
        struct sockaddr_in from;
#ifndef _WIN32
        socklen_t fromlen = sizeof from;
        ssize_t n = recvfrom(h->sock, (char *)buf, sizeof buf, 0,
                             (struct sockaddr *)&from, &fromlen);
#else
        int fromlen = sizeof from;
        int n = recvfrom(h->sock, (char *)buf, sizeof buf, 0,
                         (struct sockaddr *)&from, &fromlen);
#endif
        if (n < 12) continue;

        /* Check if this is a query (QR=0) containing PTR for _ipp or _universal */
        uint16_t flags = (uint16_t)((buf[2] << 8) | buf[3]);
        uint16_t qd = (uint16_t)((buf[4] << 8) | buf[5]);
        if (flags & 0x8000 || qd == 0) continue;  /* response or no questions */

        /* Scan questions for our service types */
        int pos = 12;
        int match = 0;
        for (uint16_t q = 0; q < qd && pos < n; q++) {
            /* skip name */
            while (pos < n && buf[pos] != 0) {
                if ((buf[pos] & 0xC0) == 0xC0) { pos += 2; goto name_done; }
                pos += 1 + buf[pos];
            }
            if (pos < n) pos++;  /* null terminator */
        name_done:
            if (pos + 4 > n) break;
            uint16_t qtype = (uint16_t)((buf[pos] << 8) | buf[pos + 1]);
            pos += 4;
            if (qtype == 12) match = 1;  /* PTR */
        }
        if (!match) continue;

        /* Send response with our records */
        int len = build_announcement(h, buf, sizeof buf, 1);
        if (len > 0)
            sendto(h->sock, (const char *)buf, (size_t)len, 0,
                   (struct sockaddr *)&from, fromlen);
    }
    return 0;
}

mdns_handle_t *mdns_start(const char *service_name, uint16_t port, const char *rp)
{
    mdns_handle_t *h;
    char hn[256];
#ifndef _WIN32
    struct sockaddr_in bind_addr;
    struct ip_mreq mreq;
    int one = 1;
#else
    WSADATA wsa;
    struct sockaddr_in bind_addr;
    struct ip_mreq mreq;
    int one = 1;
#endif

    h = calloc(1, sizeof *h);
    if (!h) return NULL;

    snprintf(h->service_name, sizeof h->service_name, "%s", service_name);
    snprintf(h->rp, sizeof h->rp, "%s", rp);

    /* Hostname: derive from service name, lowercase, no spaces */
    {
        int i;
        for (i = 0; service_name[i] && i < 60; i++) {
            char c = service_name[i];
            h->host_name[i] = (c == ' ') ? '-' : (c >= 'A' && c <= 'Z') ? c + 32 : c;
        }
        h->host_name[i] = 0;
        strcat(h->host_name, "-6125");
    }
    h->port = port;

    /* Get our primary IPv4 address */
    h->our_ip = 0;
    {
#ifndef _WIN32
        struct sockaddr_in tmp;
        int ts = socket(AF_INET, SOCK_DGRAM, 0);
        if (ts >= 0) {
            struct sockaddr_in dst;
            memset(&dst, 0, sizeof dst);
            dst.sin_family = AF_INET;
            dst.sin_port = htons(53);
            inet_pton(AF_INET, "8.8.8.8", &dst.sin_addr);
            if (connect(ts, (struct sockaddr *)&dst, sizeof dst) == 0) {
                socklen_t tl = sizeof tmp;
                getsockname(ts, (struct sockaddr *)&tmp, &tl);
                h->our_ip = tmp.sin_addr.s_addr;
            }
            close(ts);
        }
#else
        if (gethostname(hn, sizeof hn) == 0) {
            struct addrinfo hints, *res = NULL;
            memset(&hints, 0, sizeof hints);
            hints.ai_family = AF_INET;
            if (getaddrinfo(hn, NULL, &hints, &res) == 0 && res) {
                h->our_ip = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
                freeaddrinfo(res);
            }
        }
#endif
        if (h->our_ip == 0)
            inet_pton(AF_INET, "127.0.0.1", &h->our_ip);
    }

#ifdef _WIN32
    WSAStartup(MAKEWORD(2, 2), &wsa);
#else
    (void)hn;
#endif

    h->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (h->sock < 0) { free(h); return NULL; }

    setsockopt(h->sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);

    memset(&bind_addr, 0, sizeof bind_addr);
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind_addr.sin_port = htons(MDNS_PORT);
    if (bind(h->sock, (struct sockaddr *)&bind_addr, sizeof bind_addr) < 0) {
#ifdef _WIN32
        closesocket(h->sock); free(h); return NULL;
#else
        close(h->sock); free(h); return NULL;
#endif
    }

    /* Join multicast group */
    memset(&mreq, 0, sizeof mreq);
    inet_pton(AF_INET, MDNS_GROUP_V4, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = h->our_ip ? h->our_ip : INADDR_ANY;
    setsockopt(h->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof mreq);

    h->running = 1;
#ifndef _WIN32
    if (pthread_create(&h->thread, NULL, mdns_thread, h) != 0) {
        close(h->sock); free(h); return NULL;
    }
#else
    h->thread = CreateThread(NULL, 0, mdns_thread, h, 0, NULL);
    if (!h->thread) {
        closesocket(h->sock); free(h); return NULL;
    }
#endif
    return h;
}

void mdns_stop(mdns_handle_t *h)
{
    if (!h) return;
    h->running = 0;
#ifndef _WIN32
    shutdown(h->sock, SHUT_RDWR);
    pthread_join(h->thread, NULL);
    close(h->sock);
#else
    closesocket(h->sock);
    WaitForSingleObject(h->thread, 2000);
    CloseHandle(h->thread);
#endif
    free(h);
}
