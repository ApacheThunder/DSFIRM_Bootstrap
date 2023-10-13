#define ARM9
#undef ARM7

#ifndef MINICONSOLE_H
#define MINICONSOLE_H

#include <nds/memory.h>
#include <nds/arm9/background.h>

static int windowX = 0;
static int windowY = 0;
static int windowWidth = 32;
static int windowHeight = 24;
static int consoleWidth = 32;
static int tabSize = 3;
static int cursorX = 0;
static int cursorY = 0;
static u16 fontCurPal = 1;
static u16* fontBgMap = BG_MAP_RAM_SUB(4);

static void miniconsoleSetWindow(int x, int y, int width, int height) {
	windowX = x;
	windowY = y;
	windowWidth = width;
	windowHeight = height;
	cursorX = 0;
	cursorY = 0;
}

void miniNewRow() {
	cursorY ++;
	if(cursorY  >= windowHeight)  {
		int rowCount;
		int colCount;
		cursorY --;
		for(rowCount = 0; rowCount < windowHeight - 1; rowCount++)
			for(colCount = 0; colCount < windowWidth; colCount++)
				fontBgMap[(colCount + windowX) + (rowCount + windowY) * consoleWidth] =
					fontBgMap[(colCount + windowX) + (rowCount + windowY + 1) * consoleWidth];

		for(colCount = 0; colCount < windowWidth; colCount++)
			fontBgMap[(colCount + windowX) + (rowCount + windowY) * consoleWidth] =
				(' ');
	}
}

void miniconsolePrintChar(char c) {
	if (c==0) return;
	if(fontBgMap == 0) return;
	if(cursorX  >= windowWidth) { cursorX = 0; miniNewRow(); }
	switch(c) {
		case 8:
			cursorX--;
			if(cursorX < 0) {
				if(cursorY > 0) {
					cursorX = windowX - 1;
					cursorY--;
				} else {
					cursorX = 0;
				}
			}
			fontBgMap[cursorX + windowX + (cursorY + windowY) * consoleWidth] = (u16)c | fontCurPal << 12;
			break;
		case 9:
			cursorX  += tabSize - ((cursorX)%(tabSize));
			break;
		case 10:
			miniNewRow(); // fallthrough
		case 13:
			cursorX = 0;
			break;
		default:
			fontBgMap[cursorX + windowX + (cursorY + windowY) * consoleWidth] = (u16)c | fontCurPal << 12;
			++cursorX;
			break;
	}
}

void miniconsoleCls(char mode) {
	int i = 0;
	int colTemp,rowTemp;

	switch (mode) {
	case '[':
	case '0':
		{
			colTemp = cursorX ;
			rowTemp = cursorY ;

			while(i++ < ((windowHeight * windowWidth) - (rowTemp * consoleWidth + colTemp)))
				miniconsolePrintChar(' ');

			cursorX  = colTemp;
			cursorY  = rowTemp;
			break;
		}
	case '1':
		{
			colTemp = cursorX ;
			rowTemp = cursorY ;

			cursorY  = 0;
			cursorX  = 0;

			while (i++ < (rowTemp * windowWidth + colTemp))
				miniconsolePrintChar(' ');

			cursorX  = colTemp;
			cursorY  = rowTemp;
			break;
		}
	case '2':
		{
			cursorY  = 0;
			cursorX  = 0;

			while(i++ < windowHeight * windowWidth)
				miniconsolePrintChar(' ');

			cursorY  = 0;
			cursorX  = 0;
			break;
		}
	}
}

static void Print(char *str) {
	if (str == 0)return;	
	while(*str)miniconsolePrintChar(*(str++));
}


#endif // MINICONSOLE_ARM9_H

