#ifndef _SRAM_H
#define _SRAM_H

#include <avr/io.h>
#include <stdint.h>
#include "sram_defs.h"

extern void sram_write_char(uint32_t * addr, unsigned char value);
extern char sram_read_char(uint32_t * addr);
extern void sram_latch(uint32_t * addr);

#endif // _SRAM_H
