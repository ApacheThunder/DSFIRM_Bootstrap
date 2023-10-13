#include <nds.h>
#include <nds/arm9/console.h>
#include <stdio.h>
#include <fat.h>

#include "launch_engine.h"
#include "read_card.h"
#include "nds_card.h"

extern PrintConsole* currentConsole;

void DoWait(int waitTime = 30){
	for (int i = 0; i < waitTime; i++)swiWaitForVBlank();
};

void ResetSlot1() {
	if (REG_SCFG_MC == 0x11) return;
	disableSlot1();
	DoWait();
	enableSlot1();
}

void DoCartCheck() {
	if (REG_SCFG_MC == 0x11) {
		do { swiWaitForVBlank(); } while (REG_SCFG_MC != 0x10);
		enableSlot1();
		DoWait(60);
	}
}

void InitGUI() {
	// Enable console
	videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE);
	vramSetBankC(VRAM_C_SUB_BG_0x06200000);
	REG_BG0CNT_SUB = BG_MAP_BASE(4) | BG_COLOR_16 | BG_TILE_BASE(6) | BG_PRIORITY(0);
	consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 4, 6, false, true);
	// Change console palette so text is black and background is white to match top screen logo color scheme
	BG_PALETTE_SUB[0] = RGB15(31,31,31);
	BG_PALETTE_SUB[255] = RGB15(0,0,0);
	consoleSetWindow(currentConsole, 0, 11, 32, 10); // Set console position for debug text if/when needed.
}


//---------------------------------------------------------------------------------
int main(void) {
	
	InitGUI();
	
	if (!fatInitDefault()) {
		printf("         FAT init failed        ");
		while(1);
	}
	
	if (isDSiMode() && (REG_SCFG_EXT & BIT(31))) {
		bool CartWasMissing = false;
		
		if (REG_SCFG_MC != 0x18) {
			consoleClear();
			printf("        Insert Cartridge        ");
			if (!CartWasMissing)CartWasMissing = true;
		}
		while (REG_SCFG_MC != 0x18)DoCartCheck();
		
		// Delay half a second for the DS card to stabilise
		if (CartWasMissing) { 
			consoleClear();
			DoWait();
		}
		
		ALIGN(4) sNDSHeaderExt* ndsHeaderExt = (sNDSHeaderExt*)malloc(sizeof(sNDSHeaderExt));
		cardInit(ndsHeaderExt);
	}

	if(access("/firmware.nds", F_OK) != 0) {
		consoleClear();
		printf("     firmware.nds not found!    ");
		while(1);
		consoleClear();
	}
	
	// Set console to NTR ram layout (if possible) before we put stuff in ram.
	if (REG_SCFG_EXT & BIT(31)) {
		REG_SCFG_EXT &= ~(1UL << 14); // TWL RAM DISABLE 1
		REG_SCFG_EXT &= ~(1UL << 15); // TWL RAM DISABLE 2
		DoWait(10);
	}

	FILE *firmFile = fopen("/firmware.nds", "rb");
	ALIGN(4) tNDSHeader* srcNdsHeader = (tNDSHeader*)malloc(sizeof(tNDSHeader));
	fread(srcNdsHeader, sizeof(tNDSHeader), 1, firmFile);
	
	if((srcNdsHeader->arm9binarySize > 0x80000) || (srcNdsHeader->arm7binarySize > 0xC000)) {
		consoleClear();
		printf("  Firmware binaries too large!  ");
		while(1);
	}
	
	fseek(firmFile, srcNdsHeader->arm9romOffset, SEEK_SET);
	fread((void*)0x02100000, 1, srcNdsHeader->arm9binarySize, firmFile);
	DoWait(5);
	fseek(firmFile, srcNdsHeader->arm7romOffset, SEEK_SET);
	fread((void*)0x02180000, 1, srcNdsHeader->arm7binarySize, firmFile);
	
	fclose(firmFile);
	
	if (!isDSiMode() || !(REG_SCFG_EXT & BIT(31))) {
		u32 ndsHeader[0x80];
		// I don't know why you'd run this on a DS/DS Lite but you do you I suppose. :P
		consoleClear();
		printf("  Please eject flashcart now...");
		do {
			DoWait(3);
			getHeader(ndsHeader);
		} while (ndsHeader[0] != 0xffffffff);
		consoleClear();
		printf("  Please insert a cartridge...");
		do {
			DoWait(3);
			getHeader(ndsHeader);
		} while (ndsHeader[0] == 0xffffffff);
		consoleClear();
	}
	
	runLaunchEngine();
	
	while(1)swiWaitForVBlank();
	return 0;
}