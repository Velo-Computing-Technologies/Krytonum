#ifndef NET_H
#define NET_H

#include <stdint.h>

/*
 * Static network configuration — adjust before building if needed.
 * On a real LAN the OS uses 192.168.1.100/24 with gateway 192.168.1.1.
 * In QEMU (user-mode networking) the OS is 10.0.2.15/24 with gw 10.0.2.2.
 * Change these to match your LAN when booting on the Surface 4.
 */
#define NET_OUR_IP       0xC0A80164u  /* 192.168.1.100 */
#define NET_GATEWAY_IP   0xC0A80101u  /* 192.168.1.1   */
#define NET_SUBNET_MASK  0xFFFFFF00u  /* /24            */

/* Target ESP32 gateway */
#define NET_ESP32_IP     0xC0A80122u  /* 192.168.1.34  */
#define NET_ESP32_PORT   8080

/*
 * Initialise the network stack (calls e1000_init internally).
 * Returns 1 if a NIC was found and configured, 0 otherwise.
 */
void net_init(void);

/*
 * Open a TCP connection to the given IPv4 address and port.
 * Returns a socket handle >= 0 on success, -1 on failure.
 */
int tcp_connect(uint32_t dst_ip, uint16_t dst_port);

/*
 * Send data on an open TCP socket.
 * Returns number of bytes sent (== len) on success, -1 on error.
 */
int tcp_send(int sock, const void *data, int len);

/*
 * Receive data from an open TCP socket into buf (up to max_len bytes).
 * Blocks until at least 1 byte arrives or timeout expires.
 * Returns bytes received, 0 on connection closed, -1 on timeout/error.
 */
int tcp_recv(int sock, void *buf, int max_len);

/* Close a TCP socket */
void tcp_close(int sock);

/* Process pending incoming packets (call periodically in the main loop) */
void net_poll(void);

#endif /* NET_H */
