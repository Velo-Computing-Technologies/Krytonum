#ifndef E1000_H
#define E1000_H

#include <stdint.h>

/* Initialise the NIC.  Returns 1 on success, 0 if no e1000 was found. */
int  e1000_init(void);

/* Send a raw Ethernet frame.  Returns 1 on success, 0 on error. */
int  e1000_send(const void *data, uint16_t len);

/*
 * Receive a raw Ethernet frame into buf (up to max_len bytes).
 * Returns the number of bytes received, or 0 if nothing is ready.
 */
uint16_t e1000_recv(void *buf, uint16_t max_len);

/* Our MAC address (filled in by e1000_init) */
extern uint8_t e1000_mac[6];

#endif /* E1000_H */
