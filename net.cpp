/*
 * Minimal network stack: ARP + IPv4 + TCP.
 *
 * Design constraints:
 *   - Single TCP connection at a time (one socket slot).
 *   - No dynamic memory — all buffers are statically allocated.
 *   - Polling only; no interrupts.
 *   - Checksums computed in software.
 *
 * Only the subset required to perform an HTTP POST to the ESP32 gateway
 * is implemented; full RFC compliance is intentionally out of scope.
 */

#include "net.h"
#include "e1000.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Byte-order helpers (host is little-endian x86)
 * ----------------------------------------------------------------------- */
static inline uint16_t htons(uint16_t v)
{
    return (uint16_t)((v >> 8) | (v << 8));
}
static inline uint32_t htonl(uint32_t v)
{
    return ((v & 0xFF000000u) >> 24)
         | ((v & 0x00FF0000u) >>  8)
         | ((v & 0x0000FF00u) <<  8)
         | ((v & 0x000000FFu) << 24);
}
static inline uint16_t ntohs(uint16_t v) { return htons(v); }
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

/* -----------------------------------------------------------------------
 * Memory helpers (no libc)
 * ----------------------------------------------------------------------- */
static void my_memcpy(void *dst, const void *src, int n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
}
static void my_memset(void *dst, uint8_t val, int n)
{
    uint8_t *d = (uint8_t *)dst;
    while (n--) *d++ = val;
}
static int my_memcmp(const void *a, const void *b, int n)
{
    const uint8_t *p = (const uint8_t *)a;
    const uint8_t *q = (const uint8_t *)b;
    while (n--) { if (*p != *q) return *p - *q; p++; q++; }
    return 0;
}

/* -----------------------------------------------------------------------
 * Ethernet frame layout
 * ----------------------------------------------------------------------- */
#define ETH_HEADER_LEN  14
#define ETHERTYPE_ARP   0x0806
#define ETHERTYPE_IP    0x0800

struct eth_hdr {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t ethertype;   /* big-endian */
} __attribute__((packed));

static const uint8_t broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

/* -----------------------------------------------------------------------
 * ARP
 * ----------------------------------------------------------------------- */
struct arp_pkt {
    struct eth_hdr eth;
    uint16_t hw_type;     /* 1 = Ethernet */
    uint16_t proto_type;  /* 0x0800 = IPv4 */
    uint8_t  hw_len;      /* 6 */
    uint8_t  proto_len;   /* 4 */
    uint16_t op;          /* 1=request, 2=reply */
    uint8_t  sha[6];      /* sender MAC */
    uint32_t spa;         /* sender IP (big-endian) */
    uint8_t  tha[6];      /* target MAC */
    uint32_t tpa;         /* target IP (big-endian) */
} __attribute__((packed));

static uint8_t arp_cache_ip[4];      /* cached remote IP (network order) */
static uint8_t arp_cache_mac[6];     /* resolved MAC */
static int     arp_resolved = 0;

static void arp_send_request(uint32_t target_ip)
{
    struct arp_pkt pkt;
    my_memset(&pkt, 0, sizeof(pkt));

    my_memcpy(pkt.eth.dst, broadcast_mac, 6);
    my_memcpy(pkt.eth.src, e1000_mac, 6);
    pkt.eth.ethertype = htons(ETHERTYPE_ARP);

    pkt.hw_type   = htons(1);
    pkt.proto_type = htons(0x0800);
    pkt.hw_len    = 6;
    pkt.proto_len = 4;
    pkt.op        = htons(1);  /* request */

    my_memcpy(pkt.sha, e1000_mac, 6);
    pkt.spa = htonl(NET_OUR_IP);
    my_memset(pkt.tha, 0, 6);
    pkt.tpa = htonl(target_ip);

    e1000_send(&pkt, sizeof(pkt));
}

static void arp_handle_reply(const struct arp_pkt *pkt)
{
    /* Store the sender's MAC for the IP we were looking for */
    uint32_t sender_ip = ntohl(pkt->spa);
    if (my_memcmp(&sender_ip, arp_cache_ip, 4) == 0) {
        my_memcpy(arp_cache_mac, pkt->sha, 6);
        arp_resolved = 1;
    }
}

/* Resolve an IPv4 address to a MAC.  Uses cached result if available.
 * Returns 1 on success (mac filled in), 0 on timeout. */
static int arp_resolve(uint32_t ip, uint8_t mac[6])
{
    /* Check cache */
    uint32_t cached_ip;
    my_memcpy(&cached_ip, arp_cache_ip, 4);
    if (arp_resolved && cached_ip == ip) {
        my_memcpy(mac, arp_cache_mac, 6);
        return 1;
    }

    /* Store the target IP and clear cache */
    uint32_t ip_be = ip;  /* already host order — store as-is for compare */
    my_memcpy(arp_cache_ip, &ip_be, 4);
    arp_resolved = 0;

    /* Send up to 5 ARP requests with polling between them */
    for (int attempt = 0; attempt < 5; attempt++) {
        arp_send_request(ip);

        /* Poll for up to ~200 ms */
        for (int t = 0; t < 500000; t++) {
            uint8_t buf[2048];
            uint16_t len = e1000_recv(buf, sizeof(buf));
            if (len < sizeof(struct arp_pkt)) continue;

            struct arp_pkt *rpkt = (struct arp_pkt *)buf;
            if (ntohs(rpkt->eth.ethertype) != ETHERTYPE_ARP) continue;
            if (ntohs(rpkt->op) != 2) continue;  /* not a reply */

            arp_handle_reply(rpkt);
            if (arp_resolved) {
                my_memcpy(mac, arp_cache_mac, 6);
                return 1;
            }
        }
    }
    return 0;  /* timed out */
}

/* -----------------------------------------------------------------------
 * IPv4
 * ----------------------------------------------------------------------- */
struct ip_hdr {
    uint8_t  ver_ihl;   /* 0x45 = version 4, IHL 5 (20 bytes) */
    uint8_t  dscp;
    uint16_t total_len; /* big-endian, includes header */
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;  /* 6 = TCP */
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed));

#define IP_HDR_LEN  20
#define IP_PROTO_TCP 6

static uint16_t ip_id = 1;

static uint16_t checksum16(const void *data, int len)
{
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(const uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

/* Build an IPv4 header in buf (must have at least IP_HDR_LEN bytes) */
static void ip_build(uint8_t *buf, uint32_t src, uint32_t dst,
                     uint8_t proto, uint16_t payload_len)
{
    struct ip_hdr *h = (struct ip_hdr *)buf;
    h->ver_ihl    = 0x45;
    h->dscp       = 0;
    h->total_len  = htons((uint16_t)(IP_HDR_LEN + payload_len));
    h->id         = htons(ip_id++);
    h->flags_frag = htons(0x4000);  /* Don't Fragment */
    h->ttl        = 64;
    h->protocol   = proto;
    h->checksum   = 0;
    h->src_ip     = htonl(src);
    h->dst_ip     = htonl(dst);
    h->checksum   = checksum16(h, IP_HDR_LEN);
}

/* -----------------------------------------------------------------------
 * TCP
 * ----------------------------------------------------------------------- */
struct tcp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_off;   /* upper 4 bits = header len in 32-bit words */
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed));

#define TCP_HDR_LEN  20
#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_ACK  0x10

/* Single socket state */
typedef struct {
    int      in_use;
    uint32_t dst_ip;
    uint16_t dst_port;
    uint16_t src_port;
    uint32_t seq;        /* our next sequence number */
    uint32_t ack;        /* our expected next byte from remote */
    uint8_t  dst_mac[6];
    int      state;      /* 0=closed, 1=syn_sent, 2=established, 3=close_wait */
    /* RX ring buffer */
    uint8_t  rx_buf[4096];
    int      rx_head;
    int      rx_tail;
} tcp_sock_t;

static tcp_sock_t the_sock;  /* only one connection at a time */
static uint16_t next_src_port = 49152;

/* Compute the TCP checksum using the IPv4 pseudo-header */
static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                              const void *tcp_seg, uint16_t seg_len)
{
    /* Pseudo-header */
    struct {
        uint32_t src;
        uint32_t dst;
        uint8_t  zero;
        uint8_t  proto;
        uint16_t tcp_len;
    } ph;
    ph.src     = htonl(src_ip);
    ph.dst     = htonl(dst_ip);
    ph.zero    = 0;
    ph.proto   = IP_PROTO_TCP;
    ph.tcp_len = htons(seg_len);

    uint32_t sum = 0;
    const uint16_t *p;
    int n;

    p = (const uint16_t *)&ph;
    n = sizeof(ph);
    while (n > 1) { sum += *p++; n -= 2; }

    p = (const uint16_t *)tcp_seg;
    n = (int)seg_len;
    while (n > 1) { sum += *p++; n -= 2; }
    if (n) sum += *(const uint8_t *)p;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

/* Build and send a TCP segment */
#define MAX_PKT 1500
static int tcp_send_seg(tcp_sock_t *s, uint8_t flags,
                        const uint8_t *data, uint16_t data_len)
{
    static uint8_t frame[MAX_PKT + ETH_HEADER_LEN + IP_HDR_LEN + TCP_HDR_LEN];

    uint16_t seg_len = (uint16_t)(TCP_HDR_LEN + data_len);
    uint16_t ip_len  = (uint16_t)(IP_HDR_LEN + seg_len);
    uint16_t frame_len = (uint16_t)(ETH_HEADER_LEN + ip_len);

    if (frame_len > sizeof(frame)) return -1;

    /* Ethernet header */
    struct eth_hdr *eth = (struct eth_hdr *)frame;
    my_memcpy(eth->dst, s->dst_mac, 6);
    my_memcpy(eth->src, e1000_mac,  6);
    eth->ethertype = htons(ETHERTYPE_IP);

    /* IP header */
    ip_build(frame + ETH_HEADER_LEN, NET_OUR_IP, s->dst_ip,
             IP_PROTO_TCP, seg_len);

    /* TCP header */
    struct tcp_hdr *tcp =
        (struct tcp_hdr *)(frame + ETH_HEADER_LEN + IP_HDR_LEN);
    tcp->src_port = htons(s->src_port);
    tcp->dst_port = htons(s->dst_port);
    tcp->seq      = htonl(s->seq);
    tcp->ack      = htonl(s->ack);
    tcp->data_off = (uint8_t)((TCP_HDR_LEN / 4) << 4);
    tcp->flags    = flags;
    tcp->window   = htons(4096);
    tcp->checksum = 0;
    tcp->urgent   = 0;

    if (data && data_len)
        my_memcpy((uint8_t *)tcp + TCP_HDR_LEN, data, data_len);

    tcp->checksum = tcp_checksum(NET_OUR_IP, s->dst_ip,
                                 tcp, seg_len);

    return e1000_send(frame, frame_len) ? 0 : -1;
}

/* Push bytes from a received TCP segment into the socket RX ring */
static void tcp_rx_push(tcp_sock_t *s, const uint8_t *data, int len)
{
    for (int i = 0; i < len; i++) {
        int next = (s->rx_head + 1) % (int)sizeof(s->rx_buf);
        if (next == s->rx_tail) break;  /* overflow — drop */
        s->rx_buf[s->rx_head] = data[i];
        s->rx_head = next;
    }
}

/* -----------------------------------------------------------------------
 * Process a single incoming Ethernet frame
 * ----------------------------------------------------------------------- */
static void net_handle_frame(const uint8_t *frame, uint16_t len)
{
    if (len < ETH_HEADER_LEN) return;
    const struct eth_hdr *eth = (const struct eth_hdr *)frame;

    uint16_t et = ntohs(eth->ethertype);

    if (et == ETHERTYPE_ARP) {
        /* Let ARP handler see it */
        if (len >= sizeof(struct arp_pkt)) {
            const struct arp_pkt *a = (const struct arp_pkt *)frame;
            if (ntohs(a->op) == 2) arp_handle_reply(a);
        }
        return;
    }

    if (et != ETHERTYPE_IP) return;
    if (len < ETH_HEADER_LEN + IP_HDR_LEN) return;

    const struct ip_hdr *ip =
        (const struct ip_hdr *)(frame + ETH_HEADER_LEN);
    if (ip->protocol != IP_PROTO_TCP) return;
    if (ntohl(ip->dst_ip) != NET_OUR_IP) return;

    uint16_t ip_total = ntohs(ip->total_len);
    uint16_t ip_hlen  = (uint16_t)((ip->ver_ihl & 0x0F) * 4);
    if (ip_total < ip_hlen + TCP_HDR_LEN) return;

    const struct tcp_hdr *tcp =
        (const struct tcp_hdr *)(frame + ETH_HEADER_LEN + ip_hlen);
    uint16_t tcp_hlen    = (uint16_t)((tcp->data_off >> 4) * 4);
    uint16_t tcp_data_len = (uint16_t)(ip_total - ip_hlen - tcp_hlen);

    if (!the_sock.in_use) return;
    tcp_sock_t *s = &the_sock;

    /* Match our connection */
    if (ntohs(tcp->dst_port) != s->src_port) return;
    if (ntohs(tcp->src_port) != s->dst_port) return;
    if (ntohl(ip->src_ip) != s->dst_ip) return;

    uint8_t flags = tcp->flags;
    uint32_t seg_seq = ntohl(tcp->seq);
    uint32_t seg_ack = ntohl(tcp->ack);

    if (s->state == 1 /* SYN_SENT */) {
        if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
            s->ack = seg_seq + 1;
            s->seq = seg_ack;
            s->state = 2;
            tcp_send_seg(s, TCP_ACK, 0, 0);
        }
        return;
    }

    if (s->state == 2 /* ESTABLISHED */) {
        /* ACK our outstanding data */
        if (flags & TCP_ACK) s->seq = seg_ack;

        /* Deliver payload */
        if (tcp_data_len > 0) {
            const uint8_t *payload = (const uint8_t *)tcp + tcp_hlen;
            tcp_rx_push(s, payload, tcp_data_len);
            s->ack = seg_seq + tcp_data_len;
            tcp_send_seg(s, TCP_ACK, 0, 0);
        }

        /* Remote closed */
        if (flags & TCP_FIN) {
            s->ack++;
            s->state = 3;
            tcp_send_seg(s, TCP_ACK | TCP_FIN, 0, 0);
            s->seq++;
        }
        return;
    }

    if (s->state == 3 /* CLOSE_WAIT */) {
        if (flags & TCP_ACK) s->state = 0;
    }
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */
void net_init(void)
{
    my_memset(&the_sock, 0, sizeof(the_sock));
    e1000_init();
}

void net_poll(void)
{
    uint8_t buf[2048];
    uint16_t len;
    while ((len = e1000_recv(buf, sizeof(buf))) > 0)
        net_handle_frame(buf, len);
}

int tcp_connect(uint32_t dst_ip, uint16_t dst_port)
{
    tcp_sock_t *s = &the_sock;
    if (s->in_use) return -1;

    /* Decide which IP to ARP for: use gateway if off-subnet */
    uint32_t next_hop = dst_ip;
    if ((dst_ip & NET_SUBNET_MASK) != (NET_OUR_IP & NET_SUBNET_MASK))
        next_hop = NET_GATEWAY_IP;

    uint8_t mac[6];
    if (!arp_resolve(next_hop, mac)) return -1;

    my_memset(s, 0, sizeof(*s));
    s->in_use   = 1;
    s->dst_ip   = dst_ip;
    s->dst_port = dst_port;
    s->src_port = next_src_port++;
    s->seq      = 0x12345678;  /* arbitrary ISN */
    s->ack      = 0;
    s->state    = 1;  /* SYN_SENT */
    my_memcpy(s->dst_mac, mac, 6);

    /* Send SYN */
    if (tcp_send_seg(s, TCP_SYN, 0, 0) < 0) {
        s->in_use = 0;
        return -1;
    }
    s->seq++;

    /* Wait for SYN-ACK */
    for (int t = 0; t < 5000000; t++) {
        net_poll();
        if (s->state == 2) return 0;  /* connected — return socket 0 */
        if (s->state == 0) break;     /* reset */
    }
    s->in_use = 0;
    return -1;
}

int tcp_send(int sock, const void *data, int len)
{
    (void)sock;
    tcp_sock_t *s = &the_sock;
    if (!s->in_use || s->state != 2) return -1;

    /* Fragment into MSS-sized chunks */
    const uint8_t *p = (const uint8_t *)data;
    int sent = 0;
    while (sent < len) {
        uint16_t chunk = (uint16_t)(len - sent);
        if (chunk > 1400) chunk = 1400;
        if (tcp_send_seg(s, TCP_PSH | TCP_ACK, p + sent, chunk) < 0)
            return -1;
        s->seq += chunk;
        sent   += chunk;
    }
    return sent;
}

int tcp_recv(int sock, void *buf, int max_len)
{
    (void)sock;
    tcp_sock_t *s = &the_sock;
    if (!s->in_use) return -1;

    /* Poll until data or connection closed */
    for (int t = 0; t < 50000000; t++) {
        net_poll();
        if (s->rx_head != s->rx_tail) {
            int n = 0;
            uint8_t *dst = (uint8_t *)buf;
            while (n < max_len && s->rx_head != s->rx_tail) {
                dst[n++] = s->rx_buf[s->rx_tail];
                s->rx_tail = (s->rx_tail + 1) % (int)sizeof(s->rx_buf);
            }
            return n;
        }
        if (s->state == 0 || s->state == 3) return 0;  /* closed */
    }
    return -1;  /* timeout */
}

void tcp_close(int sock)
{
    (void)sock;
    tcp_sock_t *s = &the_sock;
    if (!s->in_use) return;
    if (s->state == 2)
        tcp_send_seg(s, TCP_FIN | TCP_ACK, 0, 0);
    s->in_use = 0;
    s->state  = 0;
}
