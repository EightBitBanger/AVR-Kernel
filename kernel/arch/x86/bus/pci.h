#ifndef _PCI_BUS_H_
#define _PCI_BUS_H_

#include <stdint.h>

void pci_init(void);

void* pci_map_device_bars(uint8_t bus, uint8_t dev, uint8_t func);

#endif
