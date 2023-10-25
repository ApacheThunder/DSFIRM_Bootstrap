#include <nds.h>
#include <nds/arm9/console.h>
#include <stdio.h>
#include <fat.h>
#include <limits.h>
#include <sys/stat.h>

#include "launch_engine.h"
#include "read_card.h"
#include "nds_card.h"
#include "tonccpy.h"

typedef struct sFirmData {
	u32 firmA9Location;
	u32 firmA9Destination;
	u32 firmA9Entry;
	u32 firmA9Size;
	u32 firmA7Location;
	u32 firmA7Destination;
	u32 firmA7Entry;
	u32 firmA7Size;
	u8 builtInFirm;
	u8 hasCart;
	unsigned long storedFileCluster;
} tFirmData;

tFirmData* firmData;

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
	
	bool isDSi = isDSiMode();
	
	if (!fatInitDefault()) {
		printf("         FAT init failed        ");
		while(1);
	}
	
	if (REG_SCFG_EXT & BIT(31)) {
		REG_SCFG_CLK = 0x80;
		// REG_SCFG_EXT = 0x8304E000;
		REG_SCFG_EXT = 0x83000000;
		DoWait(20);
	}

	if(access("/firmware.nds", F_OK) != 0) {
		consoleClear();
		printf("     firmware.nds not found!    ");
		while(1);
		consoleClear();
	}
	
	const char* filename = "/firmware.nds";
	struct stat st;
	char filePath[PATH_MAX];
	int pathLen;
	
	if ((!getcwd (filePath, PATH_MAX)) || (stat (filename, &st) < 0))return 0;
	
	pathLen = strlen(filename);
	strcpy (filePath + pathLen, filename);

	firmData = (tFirmData*)0x027FC000;
	firmData->builtInFirm = 0xFF;
	firmData->hasCart = 0xFF;
	firmData->storedFileCluster = st.st_ino;

	if (isDSi) {
		if (REG_SCFG_EXT & BIT(31)) {
			bool CartWasMissing = (REG_SCFG_MC == 0x11);
			if (!CartWasMissing) {
				ALIGN(4) sNDSHeaderExt* ndsHeaderExt = (sNDSHeaderExt*)malloc(sizeof(sNDSHeaderExt));
				if (REG_SCFG_MC == 0x10)enableSlot1();
				cardInit(ndsHeaderExt);
				firmData->hasCart = 0x01;
			}
		}
	} else {
		// I don't know why you'd run this on a DS/DS Lite but you do you I suppose. :P
		FILE *firmFile = fopen(filename, "rb");
		ALIGN(4) sNDSHeaderExt* ndsHeaderExt = (sNDSHeaderExt*)malloc(sizeof(sNDSHeaderExt));
		fread(ndsHeaderExt, sizeof(sNDSHeaderExt), 1, firmFile);
		
		if((ndsHeaderExt->arm9binarySize > 0x80000) || (ndsHeaderExt->arm7binarySize > 0xC000)) {
			consoleClear();
			printf("  Firmware binaries too large!  ");
			while(1);
		}
		
		firmData->firmA9Location = (u32)0x02100000;
		firmData->firmA7Location = (u32)0x02180000;
		
		fseek(firmFile, ndsHeaderExt->arm9romOffset, SEEK_SET);
		fread((void*)firmData->firmA9Location, 1, ndsHeaderExt->arm9binarySize, firmFile);
		DoWait(5);
		fseek(firmFile, ndsHeaderExt->arm7romOffset, SEEK_SET);
		fread((void*)firmData->firmA7Location, 1, ndsHeaderExt->arm7binarySize, firmFile);

		firmData->firmA9Destination = ndsHeaderExt->arm9destination;
		firmData->firmA9Entry = ndsHeaderExt->arm9executeAddress;
		firmData->firmA9Size = ndsHeaderExt->arm9binarySize;
		firmData->firmA7Destination = ndsHeaderExt->arm7destination;
		firmData->firmA7Entry = ndsHeaderExt->arm7executeAddress;
		firmData->firmA7Size = ndsHeaderExt->arm7binarySize;
		firmData->builtInFirm = 0x01;
		firmData->hasCart = 0x01;
		
		fclose(firmFile);
		
		ALIGN(4) u32 ndsHeader[0x80];
		
		consoleClear();
		printf("  Please eject flashcart now...");
		do {
			DoWait(3);
			getHeader(ndsHeader);
		} while (ndsHeader[0] != 0xFFFFFFFF);
		consoleClear();
		printf("  Please insert a cartridge...");
		do {
			DoWait(3);
			getHeader(ndsHeader);
		} while (ndsHeader[0] == 0xFFFFFFFF);
		consoleClear();
		DoWait(30);
	}
	runLaunchEngine();
	return 0;
}

