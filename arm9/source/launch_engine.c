/*
    NitroHax -- Cheat tool for the Nintendo DS
    Copyright (C) 2008  Michael "Chishm" Chisholm

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <nds.h>

#include "load_bin.h"
#include "launch_engine.h"
#define LCDC_BANK_D (vu16*)0x06860000

#define BOOTLOADER_ARM7LOCATION 0x06020000

static void vramset (volatile void* dst, u16 val, int len) {
	vu32* dst32 = (vu32*)dst;
	u32 val32 = val | (val << 16);
	for ( ; len > 0; len -= 4) { *dst32++ = val32; }
}

static void vramcpy (volatile void* dst, const void* src, int len) {
	vu32* dst32 = (vu32*)dst;
	const u32* src32 = (u32*)src;
	for ( ; len > 0; len -= 4) { *dst32++ = *src32++; }
}

void runLaunchEngine() {
	
	if (REG_SCFG_EXT & BIT(31)) {
		REG_MBK6 = 0x00003000;
		REG_MBK7 = 0x00003000;
		REG_MBK8 = 0x00003000;
		REG_SCFG_CLK = 0x80;
		REG_SCFG_EXT = 0x03000000;
		for (int i = 0; i < 30; i++)swiWaitForVBlank();
	}
	
	irqDisable(IRQ_ALL);
	// Direct CPU access to VRAM bank D
	VRAM_D_CR = VRAM_ENABLE | VRAM_D_LCD;
	
	// Clear VRAM
	vramset (LCDC_BANK_D, 0x0000, 128 * 1024);
	
	// Load the loader/patcher into the correct address
	vramcpy (LCDC_BANK_D, load_bin, load_bin_size);
	
	// Give the VRAM to the ARM7
	VRAM_D_CR = VRAM_ENABLE | VRAM_D_ARM7_0x06020000;
		
	// Reset into a passme loop
	REG_EXMEMCNT = 0xFFFF;
	*((vu32*)0x027FFFFC) = 0;
	*((vu32*)0x027FFE04) = (u32)0xE59FF018;
	*((vu32*)0x027FFE24) = (u32)0x027FFE04;
	resetARM7(BOOTLOADER_ARM7LOCATION);
	swiSoftReset();
}

