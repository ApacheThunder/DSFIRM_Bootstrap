#include <nds.h>
#include <nds/arm9/console.h>
#include <stdio.h>
#include <fat.h>
#include <limits.h>
#include <sys/stat.h>

#include "launch_engine.h"
#include "read_card.h"
#include "nds_card.h"

#define NDS_HEADER 0x027FFE00

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
		bool CartWasMissing = (REG_SCFG_MC == 0x11);
		
		if (!CartWasMissing) {
			sNDSHeaderExt* ndsHeaderExt = (sNDSHeaderExt*)NDS_HEADER;
			
			if (REG_SCFG_MC == 0x10)enableSlot1();
			cardInit(ndsHeaderExt);
		}
	}

	if(access("/firmware.nds", F_OK) != 0) {
		consoleClear();
		printf("     firmware.nds not found!    ");
		while(1);
		consoleClear();
	}
	
	// Set console to NTR ram layout (if possible) before we put stuff in ram.
	/*if (REG_SCFG_EXT & BIT(31)) {
		REG_SCFG_EXT &= ~(1UL << 14); // TWL RAM DISABLE 1
		REG_SCFG_EXT &= ~(1UL << 15); // TWL RAM DISABLE 2
		DoWait(10);
	}*/

	const char* filename = "/firmware.nds";
	struct stat st;
	char filePath[PATH_MAX];
	int pathLen;
	
	if ((!getcwd (filePath, PATH_MAX)) || (stat (filename, &st) < 0))return 0;
	
	pathLen = strlen(filename);
	strcpy (filePath + pathLen, filename);
	
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
	
	runLaunchEngine(st.st_ino);
	
	while(1)swiWaitForVBlank();
	return 0;
}