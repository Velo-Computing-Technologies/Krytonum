/*
 * Intel 82540EM (e1000) NIC driver.
 *
 * Tested with QEMU's "-device e1000" and compatible with real Intel
 * Gigabit adapters (82540/82541/82542/82543/82545/82546).
 *
 * Uses MMIO access, polling (no interrupts), one TX ring, one RX ring.
 *
 * Reference: Intel 82540EP/EM Software Developer's Manual
 *            (Intel document #317453)
 */

#include "e1000.h"
#include "pci.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * PCI identifiers
 * ----------------------------------------------------------------------- */
#define E1000_VENDOR   0x8086
#define E1000_DEV_QEMU 0x100E   /* 82540EM used by QEMU -device e1000 */

/* -----------------------------------------------------------------------
 * Register offsets (from MMIO base)
 * ----------------------------------------------------------------------- */
#define REG_CTRL    0x0000
#define REG_STATUS  0x0008
#define REG_EEPROM  0x0014
#define REG_ICR     0x00C0
#define REG_IMS     0x00D0
#define REG_IMC     0x00D8
#define REG_RCTL    0x0100
#define REG_TCTL    0x0400
#define REG_TIPG    0x0410
#define REG_RDBAL   0x2800
#define REG_RDBAH   0x2804
#define REG_RDLEN   0x2808
#define REG_RDH     0x2810
#define REG_RDT     0x2818
#define REG_TDBAL   0x3800
#define REG_TDBAH   0x3804
#define REG_TDLEN   0x3808
#define REG_TDH     0x3810
#define REG_TDT     0x3818
#define REG_MTA     0x5200
#define REG_RAL     0x5400
#define REG_RAH     0x5404

/* CTRL bits */
#define CTRL_SLU    (1u << 6)   /* Set Link Up */
#define CTRL_RST    (1u << 26)  /* Device Reset */

/* RCTL bits */
#define RCTL_EN         (1u << 1)
#define RCTL_SBP        (1u << 2)
#define RCTL_UPE        (1u << 3)   /* Unicast promisc */
#define RCTL_MPE        (1u << 4)   /* Multicast promisc */
#define RCTL_BAM        (1u << 15)  /* Broadcast Accept */
#define RCTL_BSIZE_2048 (0u << 16)
#define RCTL_SECRC      (1u << 26)  /* Strip Ethernet CRC */

/* TCTL bits */
#define TCTL_EN   (1u << 1)
#define TCTL_PSP  (1u << 3)  /* Pad Short Packets */
#define TCTL_CT   (0x0Fu << 4)
#define TCTL_COLD (0x040u << 12)

/* TX descriptor command bits */
#define TDESC_CMD_EOP  (1u << 0)
#define TDESC_CMD_IFCS (1u << 1)
#define TDESC_CMD_RS   (1u << 3)

/* RX descriptor status bits */
#define RDESC_STATUS_DD  (1u << 0)
#define RDESC_STATUS_EOP (1u << 1)

/* -----------------------------------------------------------------------
 * Descriptor structures (must be 16-byte aligned)
 * ----------------------------------------------------------------------- */
struct tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed));

struct rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));

/* -----------------------------------------------------------------------
 * Ring sizes and buffers
 * ----------------------------------------------------------------------- */
#define TX_RING_SIZE 8
#define RX_RING_SIZE 8
#define PACKET_SIZE  2048

static volatile struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static volatile struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));

static uint8_t tx_buf[TX_RING_SIZE][PACKET_SIZE] __attribute__((aligned(16)));
static uint8_t rx_buf[RX_RING_SIZE][PACKET_SIZE] __attribute__((aligned(16)));

static uint32_t mmio_base = 0;
static uint32_t tx_cur = 0;
static uint32_t rx_cur = 0;

uint8_t e1000_mac[6];

/* -----------------------------------------------------------------------
 * MMIO read / write helpers
 * ----------------------------------------------------------------------- */
static uint32_t e1000_read(uint32_t reg)
{
    volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)(mmio_base + reg);
    return *p;
}

static void e1000_write(uint32_t reg, uint32_t val)
{
    volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)(mmio_base + reg);
    *p = val;
}

/* -----------------------------------------------------------------------
 * EEPROM read (16-bit word at offset w)
 * ----------------------------------------------------------------------- */
static uint16_t eeprom_read(uint8_t w)
{
    e1000_write(REG_EEPROM, (uint32_t)w << 8 | 1u);
    uint32_t v;
    do { v = e1000_read(REG_EEPROM); } while (!(v & (1u << 4)));
    return (uint16_t)(v >> 16);
}

/* -----------------------------------------------------------------------
 * Delay loop (rough busy-wait; no timer available yet)
 * ----------------------------------------------------------------------- */
static void e1000_delay(uint32_t n)
{
    for (volatile uint32_t i = 0; i < n * 10000u; i++) {}
}

/* -----------------------------------------------------------------------
 * Public: initialise the NIC
 * ----------------------------------------------------------------------- */
int e1000_init(void)
{
    uint8_t bus, dev, func;

    if (!pci_find(E1000_VENDOR, E1000_DEV_QEMU, &bus, &dev, &func))
        return 0;   /* NIC not present */

    pci_enable_mmio_busmaster(bus, dev, func);
    mmio_base = pci_bar_mmio32(bus, dev, func, 0);
    if (!mmio_base) return 0;

    /* Reset the device */
    e1000_write(REG_CTRL, e1000_read(REG_CTRL) | CTRL_RST);
    e1000_delay(10);

    /* Set Link Up */
    e1000_write(REG_CTRL, e1000_read(REG_CTRL) | CTRL_SLU);

    /* Disable all interrupts */
    e1000_write(REG_IMC, 0xFFFFFFFF);
    e1000_read(REG_ICR);

    /* Read MAC from EEPROM */
    uint16_t w0 = eeprom_read(0);
    uint16_t w1 = eeprom_read(1);
    uint16_t w2 = eeprom_read(2);
    e1000_mac[0] = (uint8_t)(w0 & 0xFF);
    e1000_mac[1] = (uint8_t)(w0 >> 8);
    e1000_mac[2] = (uint8_t)(w1 & 0xFF);
    e1000_mac[3] = (uint8_t)(w1 >> 8);
    e1000_mac[4] = (uint8_t)(w2 & 0xFF);
    e1000_mac[5] = (uint8_t)(w2 >> 8);

    /* Multicast table array: all zeros */
    for (int i = 0; i < 128; i++)
        e1000_write(REG_MTA + i * 4, 0);

    /* Program our MAC into Receive Address register 0 */
    uint32_t ral = (uint32_t)e1000_mac[0]
                 | ((uint32_t)e1000_mac[1] <<  8)
                 | ((uint32_t)e1000_mac[2] << 16)
                 | ((uint32_t)e1000_mac[3] << 24);
    uint32_t rah = (uint32_t)e1000_mac[4]
                 | ((uint32_t)e1000_mac[5] <<  8)
                 | (1u << 31);  /* Address Valid */
    e1000_write(REG_RAL, ral);
    e1000_write(REG_RAH, rah);

    /* ------------------------------------------------------------------
     * Set up TX ring
     * ------------------------------------------------------------------ */
    for (int i = 0; i < TX_RING_SIZE; i++) {
        tx_ring[i].addr   = (uint64_t)(uintptr_t)tx_buf[i];
        tx_ring[i].status = 0xFF;  /* mark as "done" initially */
    }
    tx_cur = 0;

    e1000_write(REG_TDBAH, 0);
    e1000_write(REG_TDBAL, (uint32_t)(uintptr_t)tx_ring);
    e1000_write(REG_TDLEN, TX_RING_SIZE * 16);
    e1000_write(REG_TDH, 0);
    e1000_write(REG_TDT, 0);
    e1000_write(REG_TCTL,
        TCTL_EN | TCTL_PSP | TCTL_CT | TCTL_COLD);
    e1000_write(REG_TIPG, 0x0060200A);  /* recommended for 82540 */

    /* ------------------------------------------------------------------
     * Set up RX ring
     * ------------------------------------------------------------------ */
    for (int i = 0; i < RX_RING_SIZE; i++) {
        rx_ring[i].addr   = (uint64_t)(uintptr_t)rx_buf[i];
        rx_ring[i].status = 0;
    }
    rx_cur = 0;

    e1000_write(REG_RDBAH, 0);
    e1000_write(REG_RDBAL, (uint32_t)(uintptr_t)rx_ring);
    e1000_write(REG_RDLEN, RX_RING_SIZE * 16);
    e1000_write(REG_RDH, 0);
    e1000_write(REG_RDT, RX_RING_SIZE - 1);
    e1000_write(REG_RCTL,
        RCTL_EN | RCTL_BAM | RCTL_BSIZE_2048 | RCTL_SECRC |
        RCTL_UPE | RCTL_MPE);

    return 1;
}

/* -----------------------------------------------------------------------
 * Public: send a raw Ethernet frame
 * ----------------------------------------------------------------------- */
int e1000_send(const void *data, uint16_t len)
{
    if (!mmio_base || len > PACKET_SIZE) return 0;

    /* Wait for current descriptor to be free */
    for (int t = 0; t < 100000; t++) {
        if (tx_ring[tx_cur].status & 0xFF) break;
    }
    if (!(tx_ring[tx_cur].status & 0xFF)) return 0;  /* timed out */

    /* Copy payload into the TX buffer */
    const uint8_t *src = (const uint8_t *)data;
    uint8_t *dst = tx_buf[tx_cur];
    for (uint16_t i = 0; i < len; i++) dst[i] = src[i];

    tx_ring[tx_cur].length = len;
    tx_ring[tx_cur].cmd    = TDESC_CMD_EOP | TDESC_CMD_IFCS | TDESC_CMD_RS;
    tx_ring[tx_cur].status = 0;

    uint32_t old = tx_cur;
    tx_cur = (tx_cur + 1) % TX_RING_SIZE;
    e1000_write(REG_TDT, tx_cur);

    /* Wait for descriptor writeback */
    for (int t = 0; t < 100000; t++) {
        if (tx_ring[old].status & 0xFF) break;
    }
    return (tx_ring[old].status & 0xFF) ? 1 : 0;
}

/* -----------------------------------------------------------------------
 * Public: receive a raw Ethernet frame
 * ----------------------------------------------------------------------- */
uint16_t e1000_recv(void *buf, uint16_t max_len)
{
    if (!mmio_base) return 0;
    if (!(rx_ring[rx_cur].status & RDESC_STATUS_DD)) return 0;

    uint16_t len = rx_ring[rx_cur].length;
    if (len > max_len) len = max_len;

    const uint8_t *src = rx_buf[rx_cur];
    uint8_t *dst = (uint8_t *)buf;
    for (uint16_t i = 0; i < len; i++) dst[i] = src[i];

    /* Give the descriptor back to the hardware */
    rx_ring[rx_cur].status = 0;
    uint32_t old = rx_cur;
    rx_cur = (rx_cur + 1) % RX_RING_SIZE;
    e1000_write(REG_RDT, old);

    return len;
}
