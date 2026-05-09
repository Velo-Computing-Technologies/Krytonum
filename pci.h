#ifndef PCI_H
#define PCI_H

#include <stdint.h>

/* Read/write a 32-bit PCI configuration register */
uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off);
void     pci_write32(uint8_t bus, uint8_t dev, uint8_t func,
                     uint8_t off, uint32_t val);
uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off);
uint8_t  pci_read8 (uint8_t bus, uint8_t dev, uint8_t func, uint8_t off);

/* Find the first device matching vendor/device ID.
 * Returns 1 on success, 0 if not found.
 * Fills *bus_out, *dev_out, *func_out. */
int pci_find(uint16_t vendor, uint16_t device,
             uint8_t *bus_out, uint8_t *dev_out, uint8_t *func_out);

/* Return the 32-bit MMIO base address from BAR n (strips the low bits) */
uint32_t pci_bar_mmio32(uint8_t bus, uint8_t dev, uint8_t func, int bar_n);

/* Enable MMIO + bus-master in the command register */
void pci_enable_mmio_busmaster(uint8_t bus, uint8_t dev, uint8_t func);

#endif /* PCI_H */
