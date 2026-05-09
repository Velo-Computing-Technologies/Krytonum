/*
 * Minimal PCI configuration-space access using legacy I/O ports
 * (Port 0xCF8 = CONFIG_ADDRESS, 0xCFC = CONFIG_DATA).
 * Works for any x86 with a PCI host bridge (QEMU, Surface, etc.).
 */

#include "pci.h"
#include "io.h"

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC

static uint32_t pci_make_addr(uint8_t bus, uint8_t dev,
                               uint8_t func, uint8_t off)
{
    return (1u << 31)
         | ((uint32_t)bus  << 16)
         | ((uint32_t)dev  << 11)
         | ((uint32_t)func <<  8)
         | (off & 0xFC);
}

uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off)
{
    outl(PCI_ADDR, pci_make_addr(bus, dev, func, off));
    return inl(PCI_DATA);
}

void pci_write32(uint8_t bus, uint8_t dev, uint8_t func,
                 uint8_t off, uint32_t val)
{
    outl(PCI_ADDR, pci_make_addr(bus, dev, func, off));
    outl(PCI_DATA, val);
}

uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off)
{
    uint32_t v = pci_read32(bus, dev, func, off & 0xFC);
    return (uint16_t)(v >> ((off & 2) * 8));
}

uint8_t pci_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off)
{
    uint32_t v = pci_read32(bus, dev, func, off & 0xFC);
    return (uint8_t)(v >> ((off & 3) * 8));
}

int pci_find(uint16_t vendor, uint16_t device,
             uint8_t *bus_out, uint8_t *dev_out, uint8_t *func_out)
{
    for (int bus = 0; bus < 256; bus++) {
        for (int dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read32((uint8_t)bus, (uint8_t)dev, 0, 0);
            if ((id & 0xFFFF) == 0xFFFF) continue;   /* no device */
            if ((uint16_t)(id & 0xFFFF)         == vendor &&
                (uint16_t)((id >> 16) & 0xFFFF)  == device)
            {
                *bus_out  = (uint8_t)bus;
                *dev_out  = (uint8_t)dev;
                *func_out = 0;
                return 1;
            }
            /* Check multi-function cards */
            uint8_t hdr = pci_read8((uint8_t)bus, (uint8_t)dev, 0, 0x0E);
            if (hdr & 0x80) {
                for (int func = 1; func < 8; func++) {
                    uint32_t fid = pci_read32((uint8_t)bus, (uint8_t)dev,
                                              (uint8_t)func, 0);
                    if ((fid & 0xFFFF) == 0xFFFF) continue;
                    if ((uint16_t)(fid & 0xFFFF)        == vendor &&
                        (uint16_t)((fid >> 16) & 0xFFFF) == device)
                    {
                        *bus_out  = (uint8_t)bus;
                        *dev_out  = (uint8_t)dev;
                        *func_out = (uint8_t)func;
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

uint32_t pci_bar_mmio32(uint8_t bus, uint8_t dev, uint8_t func, int bar_n)
{
    uint8_t off = (uint8_t)(0x10 + bar_n * 4);
    uint32_t bar = pci_read32(bus, dev, func, off);
    return bar & ~0xFu;  /* strip lower 4 flag bits */
}

void pci_enable_mmio_busmaster(uint8_t bus, uint8_t dev, uint8_t func)
{
    uint32_t cmd = pci_read32(bus, dev, func, 0x04);
    cmd |= (1u << 1) | (1u << 2);   /* Memory Space | Bus Master */
    pci_write32(bus, dev, func, 0x04, cmd);
}
