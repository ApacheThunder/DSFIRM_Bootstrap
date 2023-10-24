/*
 main.arm7.c

 By Michael Chisholm (Chishm)

 All resetMemory and startBinary functions are based
 on the MultiNDS loader by Darkain.
 Original source available at:
 http://cvs.sourceforge.net/viewcvs.py/ndslib/ndslib/examples/loader/boot/main.cpp

 License:
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

#ifndef ARM7
# define ARM7
#endif
#include <nds/ndstypes.h>
#include <nds/system.h>
#include <nds/interrupts.h>
#include <nds/timers.h>
#include <nds/dma.h>
#include <nds/arm7/audio.h>
#include <nds/arm7/codec.h>
#include <nds/memory.h>
#include <nds/ipc.h>
#include <string.h>

#ifndef NULL
#define NULL 0
#endif

#include "common.h"
#include "read_card.h"
#include "tonccpy.h"
#include "my_fat.h"
/*-------------------------------------------------------------------------
External functions
--------------------------------------------------------------------------*/
extern void arm7_clearmem (void* loc, size_t len);
extern void arm7_reset (void);

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Important things
#define NDS_HEADER			0x027FFE00
#define NDS_HEADER_POKEMON	0x027FF000
#define TWL_HEADER			0x027FE000
#define TMP_HEADER			0x027FC000
// #define NTR_CARTARM9 0x02320000 // This is actually the firmware's bin location?
// #define NTR_CARTARM9ALT 0x02200000
// #define NTR_CARTARM9 0x02004000
#define NTR_CARTARM9 0x02200000
#define NTR_CARTARM7 0x022C0000
#define REG_GPIO_WIFI *(vu16*)0x4004C04
#define BASE_DELAY (100)

tNDSHeader* ndsHeader;
tNDSHeader* tmpHeader = (tNDSHeader*)NDS_HEADER_POKEMON;


static u32 chipID;
extern unsigned long storedFileCluster;

void arm7clearRAM();

void EnableSlot1() {
	int oldIME = enterCriticalSection();
	while((REG_SCFG_MC & 0x0c) == 0x0c) swiDelay(1 * BASE_DELAY);

	if(!(REG_SCFG_MC & 0x0c)) {
		REG_SCFG_MC = (REG_SCFG_MC & ~0x0c) | 4;
		swiDelay(10 * BASE_DELAY);
		REG_SCFG_MC = (REG_SCFG_MC & ~0x0c) | 8;
		swiDelay(10 * BASE_DELAY);
	}
	leaveCriticalSection(oldIME);
}

static tNDSHeader* loadHeader(tDSiHeader* twlHeaderTemp) {
	tNDSHeader* ntrHeader = (tNDSHeader*)NDS_HEADER;
	*ntrHeader = twlHeaderTemp->ndshdr;
	if (ntrHeader->unitCode > 0) {
		tDSiHeader* dsiHeader = (tDSiHeader*)TWL_HEADER; // __DSiHeader
		*dsiHeader = *twlHeaderTemp;
	}
	return ntrHeader;
}

static void NDSTouchscreenMode(void) {
	u8 volLevel = 0xA7;

	// Touchscreen
	cdcReadReg (0x63, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x3A, 0x00);
	cdcReadReg (CDC_CONTROL, 0x51);
	cdcReadReg (CDC_TOUCHCNT, 0x02);
	cdcReadReg (CDC_CONTROL, 0x3F);
	cdcReadReg (CDC_SOUND, 0x28);
	cdcReadReg (CDC_SOUND, 0x2A);
	cdcReadReg (CDC_SOUND, 0x2E);
	cdcWriteReg(CDC_CONTROL, 0x52, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x40, 0x0C);
	cdcWriteReg(CDC_SOUND, 0x24, 0xFF);
	cdcWriteReg(CDC_SOUND, 0x25, 0xFF);
	cdcWriteReg(CDC_SOUND, 0x26, 0x7F);
	cdcWriteReg(CDC_SOUND, 0x27, 0x7F);
	cdcWriteReg(CDC_SOUND, 0x28, 0x4A);
	cdcWriteReg(CDC_SOUND, 0x29, 0x4A);
	cdcWriteReg(CDC_SOUND, 0x2A, 0x10);
	cdcWriteReg(CDC_SOUND, 0x2B, 0x10);
	cdcWriteReg(CDC_CONTROL, 0x51, 0x00);
	cdcReadReg (CDC_TOUCHCNT, 0x02);
	cdcWriteReg(CDC_TOUCHCNT, 0x02, 0x98);
	cdcWriteReg(CDC_SOUND, 0x23, 0x00);
	cdcWriteReg(CDC_SOUND, 0x1F, 0x14);
	cdcWriteReg(CDC_SOUND, 0x20, 0x14);
	cdcWriteReg(CDC_CONTROL, 0x3F, 0x00);
	cdcReadReg (CDC_CONTROL, 0x0B);
	cdcWriteReg(CDC_CONTROL, 0x05, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x0B, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x0C, 0x02);
	cdcWriteReg(CDC_CONTROL, 0x12, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x13, 0x02);
	cdcWriteReg(CDC_SOUND, 0x2E, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x3A, 0x60);
	cdcWriteReg(CDC_CONTROL, 0x01, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x39, 0x66);
	cdcReadReg (CDC_SOUND, 0x20);
	cdcWriteReg(CDC_SOUND, 0x20, 0x10);
	cdcWriteReg(CDC_CONTROL, 0x04, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x12, 0x81);
	cdcWriteReg(CDC_CONTROL, 0x13, 0x82);
	cdcWriteReg(CDC_CONTROL, 0x51, 0x82);
	cdcWriteReg(CDC_CONTROL, 0x51, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x04, 0x03);
	cdcWriteReg(CDC_CONTROL, 0x05, 0xA1);
	cdcWriteReg(CDC_CONTROL, 0x06, 0x15);
	cdcWriteReg(CDC_CONTROL, 0x0B, 0x87);
	cdcWriteReg(CDC_CONTROL, 0x0C, 0x83);
	cdcWriteReg(CDC_CONTROL, 0x12, 0x87);
	cdcWriteReg(CDC_CONTROL, 0x13, 0x83);
	cdcReadReg (CDC_TOUCHCNT, 0x10);
	cdcWriteReg(CDC_TOUCHCNT, 0x10, 0x08);
	cdcWriteReg(0x04, 0x08, 0x7F);
	cdcWriteReg(0x04, 0x09, 0xE1);
	cdcWriteReg(0x04, 0x0A, 0x80);
	cdcWriteReg(0x04, 0x0B, 0x1F);
	cdcWriteReg(0x04, 0x0C, 0x7F);
	cdcWriteReg(0x04, 0x0D, 0xC1);
	cdcWriteReg(CDC_CONTROL, 0x41, 0x08);
	cdcWriteReg(CDC_CONTROL, 0x42, 0x08);
	cdcWriteReg(CDC_CONTROL, 0x3A, 0x00);
	cdcWriteReg(0x04, 0x08, 0x7F);
	cdcWriteReg(0x04, 0x09, 0xE1);
	cdcWriteReg(0x04, 0x0A, 0x80);
	cdcWriteReg(0x04, 0x0B, 0x1F);
	cdcWriteReg(0x04, 0x0C, 0x7F);
	cdcWriteReg(0x04, 0x0D, 0xC1);
	cdcWriteReg(CDC_SOUND, 0x2F, 0x2B);
	cdcWriteReg(CDC_SOUND, 0x30, 0x40);
	cdcWriteReg(CDC_SOUND, 0x31, 0x40);
	cdcWriteReg(CDC_SOUND, 0x32, 0x60);
	cdcReadReg (CDC_CONTROL, 0x74);
	cdcWriteReg(CDC_CONTROL, 0x74, 0x02);
	cdcReadReg (CDC_CONTROL, 0x74);
	cdcWriteReg(CDC_CONTROL, 0x74, 0x10);
	cdcReadReg (CDC_CONTROL, 0x74);
	cdcWriteReg(CDC_CONTROL, 0x74, 0x40);
	cdcWriteReg(CDC_SOUND, 0x21, 0x20);
	cdcWriteReg(CDC_SOUND, 0x22, 0xF0);
	cdcReadReg (CDC_CONTROL, 0x51);
	cdcReadReg (CDC_CONTROL, 0x3F);
	cdcWriteReg(CDC_CONTROL, 0x3F, 0xD4);
	cdcWriteReg(CDC_SOUND, 0x23, 0x44);
	cdcWriteReg(CDC_SOUND, 0x1F, 0xD4);
	cdcWriteReg(CDC_SOUND, 0x28, 0x4E);
	cdcWriteReg(CDC_SOUND, 0x29, 0x4E);
	cdcWriteReg(CDC_SOUND, 0x24, 0x9E);
	cdcWriteReg(CDC_SOUND, 0x25, 0x9E);
	cdcWriteReg(CDC_SOUND, 0x20, 0xD4);
	cdcWriteReg(CDC_SOUND, 0x2A, 0x14);
	cdcWriteReg(CDC_SOUND, 0x2B, 0x14);
	cdcWriteReg(CDC_SOUND, 0x26, 0xA7);
	cdcWriteReg(CDC_SOUND, 0x27, 0xA7);
	cdcWriteReg(CDC_CONTROL, 0x40, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x3A, 0x60);
	cdcWriteReg(CDC_SOUND, 0x26, volLevel);
	cdcWriteReg(CDC_SOUND, 0x27, volLevel);
	cdcWriteReg(CDC_SOUND, 0x2E, 0x03);
	cdcWriteReg(CDC_TOUCHCNT, 0x03, 0x00);
	cdcWriteReg(CDC_SOUND, 0x21, 0x20);
	cdcWriteReg(CDC_SOUND, 0x22, 0xF0);
	cdcReadReg (CDC_SOUND, 0x22);
	cdcWriteReg(CDC_SOUND, 0x22, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x52, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x51, 0x00);
	
	// Set remaining values
	cdcWriteReg(CDC_CONTROL, 0x03, 0x44);
	cdcWriteReg(CDC_CONTROL, 0x0D, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x0E, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x0F, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x10, 0x08);
	cdcWriteReg(CDC_CONTROL, 0x14, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x15, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x16, 0x04);
	cdcWriteReg(CDC_CONTROL, 0x1A, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x1E, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x24, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x33, 0x34);
	cdcWriteReg(CDC_CONTROL, 0x34, 0x32);
	cdcWriteReg(CDC_CONTROL, 0x35, 0x12);
	cdcWriteReg(CDC_CONTROL, 0x36, 0x03);
	cdcWriteReg(CDC_CONTROL, 0x37, 0x02);
	cdcWriteReg(CDC_CONTROL, 0x38, 0x03);
	cdcWriteReg(CDC_CONTROL, 0x3C, 0x19);
	cdcWriteReg(CDC_CONTROL, 0x3D, 0x05);
	cdcWriteReg(CDC_CONTROL, 0x44, 0x0F);
	cdcWriteReg(CDC_CONTROL, 0x45, 0x38);
	cdcWriteReg(CDC_CONTROL, 0x49, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x4A, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x4B, 0xEE);
	cdcWriteReg(CDC_CONTROL, 0x4C, 0x10);
	cdcWriteReg(CDC_CONTROL, 0x4D, 0xD8);
	cdcWriteReg(CDC_CONTROL, 0x4E, 0x7E);
	cdcWriteReg(CDC_CONTROL, 0x4F, 0xE3);
	cdcWriteReg(CDC_CONTROL, 0x58, 0x7F);
	cdcWriteReg(CDC_CONTROL, 0x74, 0xD2);
	cdcWriteReg(CDC_CONTROL, 0x75, 0x2C);
	cdcWriteReg(CDC_SOUND, 0x22, 0x70);
	cdcWriteReg(CDC_SOUND, 0x2C, 0x20);

	// Finish up!
	cdcReadReg (CDC_TOUCHCNT, 0x02);
	cdcWriteReg(CDC_TOUCHCNT, 0x02, 0x98);
	cdcWriteReg(0xFF, 0x05, 0x00); //writeTSC(0x00, 0xFF);

	// Power management
	writePowerManagement(PM_READ_REGISTER, 0x00); //*(unsigned char*)0x40001C2 = 0x80, 0x00; // read PWR[0]   ;<-- also part of TSC !
	writePowerManagement(PM_CONTROL_REG, 0x0D); //*(unsigned char*)0x40001C2 = 0x00, 0x0D; // PWR[0]=0Dh    ;<-- also part of TSC !
}

const char* getRomTid(const tNDSHeader* ndsHeader) {
	static char romTid[5];
	strncpy(romTid, ndsHeader->gameCode, 4);
	romTid[4] = '\0';
	return romTid;
}

static void errorOutput (u32 code, bool isError) {
	arm9_errorCode = code;
	if (isError) { 
		ipcSendState(ARM7_ERR);
		while(1); // Stop
	}
}

void arm7_readFirmware (uint32 address, uint8 * buffer, uint32 size) {
  uint32 index;
  u16 FW_READ = 0x03;

  // Read command
  while (REG_SPICNT & SPI_BUSY);
  REG_SPICNT = SPI_ENABLE | SPI_CONTINUOUS | SPI_DEVICE_NVRAM;
  REG_SPIDATA = FW_READ;
  while (REG_SPICNT & SPI_BUSY);

  // Set the address
  REG_SPIDATA =  (address>>16) & 0xFF;
  while (REG_SPICNT & SPI_BUSY);
  REG_SPIDATA =  (address>>8) & 0xFF;
  while (REG_SPICNT & SPI_BUSY);
  REG_SPIDATA =  (address) & 0xFF;
  while (REG_SPICNT & SPI_BUSY);

  for (index = 0; index < size; index++) {
    REG_SPIDATA = 0;
    while (REG_SPICNT & SPI_BUSY);
    buffer[index] = REG_SPIDATA & 0xFF;
  }
  REG_SPICNT = 0;
}

void arm7_resetMemory () {
	int i, reg;
	u8 settings1, settings2;

	REG_IME = 0;

	for (i=0; i<16; i++) {
		SCHANNEL_CR(i) = 0;
		SCHANNEL_TIMER(i) = 0;
		SCHANNEL_SOURCE(i) = 0;
		SCHANNEL_LENGTH(i) = 0;
	}
	REG_SOUNDCNT = 0;

	// Clear out ARM7 DMA channels and timers
	for (i=0; i<4; i++) {
		DMA_CR(i) = 0;
		DMA_SRC(i) = 0;
		DMA_DEST(i) = 0;
		TIMER_CR(i) = 0;
		TIMER_DATA(i) = 0;
		if ((REG_SNDEXTCNT != 0) && (REG_SCFG_EXT & BIT(31)))for(reg=0; reg<0x1c; reg+=4)*((u32*)(0x04004104 + ((i*0x1c)+reg))) = 0; //Reset NDMA.
	}

	// Clear out FIFO
	REG_IPC_SYNC = 0;
	REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
	REG_IPC_FIFO_CR = 0;

	arm7clearRAM();
	
	arm7_clearmem((u32*)NDS_HEADER, 0x170);
	arm7_clearmem((u32*)NDS_HEADER_POKEMON, 0x170);
	arm7_clearmem((u32*)TWL_HEADER, 0x170);
	arm7_clearmem((u32*)TMP_HEADER, 0x170);
	
	REG_IE = 0;
	REG_IF = ~0;
	if (REG_SNDEXTCNT != 0) { REG_AUXIE = 0; REG_AUXIF = ~0; }
	
	(*(vu32*)(0x04000000-4)) = 0;  //IRQ_HANDLER ARM7 version
	(*(vu32*)(0x04000000-8)) = ~0; //VBLANK_INTR_WAIT_FLAGS, ARM7 version
	
	REG_POWERCNT = 1;  //turn off power to stuffs
	
	// Reload DS Firmware settings
	arm7_readFirmware((u32)0x03FE70, &settings1, 0x1);
	arm7_readFirmware((u32)0x03FF70, &settings2, 0x1);

	if (settings1 > settings2) {
		arm7_readFirmware((u32)0x03FE00, (u8*)0x027FFC80, 0x70);
		arm7_readFirmware((u32)0x03FF00, (u8*)0x027FFD80, 0x70);
	} else {
		arm7_readFirmware((u32)0x03FF00, (u8*)0x027FFC80, 0x70);
		arm7_readFirmware((u32)0x03FE00, (u8*)0x027FFD80, 0x70);
	}

	// Load FW header
	arm7_readFirmware((u32)0x000000, (u8*)0x027FF830, 0x20);
}

void arm7_loadFirmBinary (aFile* file) {
	u32 firmHeader[0x170>>2];

	// read NDS header
	fileRead ((char*)firmHeader, file, 0, 0x40);
	// read ARM9 info from NDS header
	u32 ARM9_SRC = firmHeader[0x020>>2];
	char* ARM9_DST = (char*)firmHeader[0x028>>2];
	u32 ARM9_LEN = firmHeader[0x02C>>2];
	// read ARM7 info from NDS header
	u32 ARM7_SRC = firmHeader[0x030>>2];
	char* ARM7_DST = (char*)firmHeader[0x038>>2];
	u32 ARM7_LEN = firmHeader[0x03C>>2];
	
	// Load binaries into memory
	fileRead(ARM9_DST, file, ARM9_SRC, ARM9_LEN);
	fileRead(ARM7_DST, file, ARM7_SRC, ARM7_LEN);

	// first copy the header to its proper location, excluding
	// the ARM9 start address, so as not to start it
	*((vu32*)0x027FC024) = firmHeader[0x024>>2];
	*((vu32*)0x027FC034) = firmHeader[0x034>>2];
}

u32 arm7_loadBinary (void) {
	u32 errorCode;
	
	tDSiHeader* twlHeaderTemp = (tDSiHeader*)NDS_HEADER_POKEMON; // Use same region cheat engine goes. Cheat engine will replace this later when it's not needed.

	// Init card
	errorCode = cardInit((tNDSHeaderExt*)twlHeaderTemp, &chipID);
	if (errorCode)return errorCode;

	ndsHeader = loadHeader(twlHeaderTemp); // copy twlHeaderTemp to ndsHeader location

	if ((u32)ndsHeader->arm9destination != 0x02000000) {
		cardRead(ndsHeader->arm9romOffset, (u32*)NTR_CARTARM9, ndsHeader->arm9binarySize);
	} else {
		cardRead(ndsHeader->arm9romOffset, (u32*)ndsHeader->arm9destination, ndsHeader->arm9binarySize);
	}
	cardRead(ndsHeader->arm7romOffset, (u32*)NTR_CARTARM7, ndsHeader->arm7binarySize);
	
	// Fix Pokemon games needing header data.
	copyLoop((u32*)NDS_HEADER_POKEMON, (u32*)NDS_HEADER, 0x170);
	copyLoop((u32*)0x023FFE00, (u32*)NDS_HEADER, 0x170);

	char* romTid = (char*)NDS_HEADER_POKEMON+0xC;
	if (   memcpy(romTid, "ADA", 3) == 0 // Diamond
		|| memcmp(romTid, "APA", 3) == 0 // Pearl
		|| memcmp(romTid, "CPU", 3) == 0 // Platinum
		|| memcmp(romTid, "IPK", 3) == 0 // HG
		|| memcmp(romTid, "IPG", 3) == 0 // SS
	) {
		// Make the Pokemon game code ADAJ.
		const char gameCodePokemon[] = { 'A', 'D', 'A', 'J' };
		memcpy((char*)NDS_HEADER_POKEMON+0xC, gameCodePokemon, 4);
	}
	
	return ERR_NONE;
}

static void setMemoryAddress(const tNDSHeader* ndsHeader) {
	if (ndsHeader->unitCode > 0) {
		copyLoop((u32*)0x027FFA80, (u32*)ndsHeader, 0x160);	// Make a duplicate of DS header

		*(u32*)(0x027FA680) = 0x02FD4D80;
		*(u32*)(0x027FA684) = 0x00000000;
		*(u32*)(0x027FA688) = 0x00001980;

		*(u32*)(0x027FF00C) = 0x0000007F;
		*(u32*)(0x027FF010) = 0x550E25B8;
		*(u32*)(0x027FF014) = 0x027F4000;

		// Set region flag
		if (strncmp(getRomTid(ndsHeader)+3, "J", 1) == 0) {
			*(u8*)(0x027FFD70) = 0;
		} else if (strncmp(getRomTid(ndsHeader)+3, "E", 1) == 0) {
			*(u8*)(0x027FFD70) = 1;
		} else if (strncmp(getRomTid(ndsHeader)+3, "P", 1) == 0) {
			*(u8*)(0x027FFD70) = 2;
		} else if (strncmp(getRomTid(ndsHeader)+3, "U", 1) == 0) {
			*(u8*)(0x027FFD70) = 3;
		} else if (strncmp(getRomTid(ndsHeader)+3, "C", 1) == 0) {
			*(u8*)(0x027FFD70) = 4;
		} else if (strncmp(getRomTid(ndsHeader)+3, "K", 1) == 0) {
			*(u8*)(0x027FFD70) = 5;
		}
	}
	
    // Set memory values expected by loaded NDS
    // from NitroHax, thanks to Chism
	*((u32*)0x027FF800) = chipID;					// CurrentCardID
	*((u32*)0x027FF804) = chipID;					// Command10CardID
	*((u16*)0x027FF808) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)0x027FF80A) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((u16*)0x027FF850) = 0x5835;
	// Extra bits
	*((u16*)0x027FF869) = 0x03FE;
	*((u16*)0x027FF874) = 0x4F5D;
	 *((u8*)0x027FF880) = 0x03;
	 *((u8*)0x027FF884) = 0x02;
	*((u32*)0x027FF890) = 0x30002A02;
	
	// Copies of above
	*((u32*)0x027FFC00) = chipID;					// CurrentCardID
	*((u32*)0x027FFC04) = chipID;					// Command10CardID
	*((u16*)0x027FFC08) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)0x027FFC0A) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((u16*)0x027FFC10) = 0x5835;
	*((u16*)0x027FFC40) = 0x01;						// Boot Indicator -- EXTREMELY IMPORTANT!!! Thanks to cReDiAr
	
	tonccpy((void*)0x027FF860, (void*)0x027FFE34, 0x4);
	
	// Smaller copy of header? This is what's present in memory during DS firmware boot up at least...
	// copyLoop((u32*)0x0235621C, (u32*)NDS_HEADER, 0xE0);
	// *((u32*)0x0235621C) = 0xFFFFFFFF;
	copyLoop((u32*)0x023FF000, (u32*)0x027FF000, 0x1000);
	
	(*(vu32*)0x027FFFF4) = 0;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main function
void arm7_main (void) {
	
	u32 errorCode;
	bool noFile = false;
	bool noCart = ((REG_SCFG_MC == 0x11) || (REG_SCFG_MC == 0x10));

	// Init card
	if(!FAT_InitFiles(true))noFile = true;
	
	aFile file;

	if (!noFile) {
		getFileFromCluster(&file, storedFileCluster);
		if ((file.firstCluster < CLUSTER_FIRST) || (file.firstCluster >= CLUSTER_EOF) || (file.firstCluster == CLUSTER_FREE))noFile = true;
	}
		
	if (REG_SCFG_EXT & BIT(31)) {
		/* *((vu32*)REG_MBK1)=0x8D898581;
		*((vu32*)REG_MBK2)=0x8C888480;
		*((vu32*)REG_MBK3)=0x9C989490;
		*((vu32*)REG_MBK4)=0x8C888480;
		*((vu32*)REG_MBK5)=0x9C989490;*/
		REG_MBK6=0x09403900;
		REG_MBK7=0x09803940;
		REG_MBK8=0x09C03980;
		REG_MBK9=0xFCFFFF0F;
	}
	
	// Synchronise start
	while (ipcRecvState() != ARM9_START);
	ipcSendState(ARM7_START);

	// Wait until ARM9 is ready
	while (ipcRecvState() != ARM9_READY);

	errorOutput(ERR_STS_CLR_MEM, false);

	ipcSendState(ARM7_MEMCLR);

	// Get ARM7 to clear RAM
	arm7_resetMemory(noCart);
	
	if (REG_SNDEXTCNT != 0) {
		if (REG_SCFG_EXT & BIT(31))REG_SCFG_ROM = 0x703;
	}

	errorOutput(ERR_STS_LOAD_BIN, false);
	
	ipcSendState(ARM7_LOADBIN);

	if (noFile)errorOutput(ERR_FIRMNOTFOUND, true);

	// Load the NDS file
	if (!noFile)arm7_loadFirmBinary(&file);
	
	if (!noCart) {
		errorCode = arm7_loadBinary();
		if (errorCode)errorOutput(errorCode, true);
	}
	
	if (REG_SNDEXTCNT != 0 && (REG_SCFG_EXT & BIT(31))) {
		if (cdcReadReg(CDC_SOUND, 0x22) == 0xF0) {
			// Switch touch mode to NTR
			*(u16*)0x4004700 = 0x800F;
			NDSTouchscreenMode();
			*(u16*)0x4000500 = 0x807F;
		}
		REG_GPIO_WIFI |= BIT(8);	// Old NDS-Wifi mode
		REG_SCFG_CLK = 0x100;
		REG_SCFG_EXT = 0x12A00000;
	}
	
	errorOutput(ERR_STS_STARTBIN, false);

	if (!noCart)setMemoryAddress(ndsHeader);
	
	ipcSendState(ARM7_BOOTBIN);

	arm7_reset();
}

