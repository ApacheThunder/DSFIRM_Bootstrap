/*-----------------------------------------------------------------
 fat.c
 
 NDS MP
 GBAMP NDS Firmware Hack Version 2.12
 An NDS aware firmware patch for the GBA Movie Player.
 By Michael Chisholm (Chishm)
 
 Filesystem code based on GBAMP_CF.c by Chishm (me).
 
 License:
    Copyright (C) 2005  Michael "Chishm" Chisholm

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

    If you use this code, please give due credit and email me about your
    project at chishm@hotmail.com
------------------------------------------------------------------*/
   
#include "tonccpy.h"
#include "my_fat.h"
#include "card.h"

#define nextClusterBufferCount 8

//#define memcpy __builtin_memcpy


//---------------------------------------------------------------
// FAT constants

#define FILE_LAST 0x00
#define FILE_FREE 0xE5

#define ATTRIB_ARCH	0x20
#define ATTRIB_DIR	0x10
#define ATTRIB_LFN	0x0F
#define ATTRIB_VOL	0x08
#define ATTRIB_HID	0x02
#define ATTRIB_SYS	0x04
#define ATTRIB_RO	0x01

#define FAT16_ROOT_DIR_CLUSTER 0x00

// File Constants
#ifndef EOF
#define EOF -1
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2
#endif


//-----------------------------------------------------------------
// FAT constants
#define CLUSTER_EOF_16	0xFFFF

#define ATTRIB_ARCH	0x20
#define ATTRIB_DIR	0x10
#define ATTRIB_LFN	0x0F
#define ATTRIB_VOL	0x08
#define ATTRIB_HID	0x02
#define ATTRIB_SYS	0x04
#define ATTRIB_RO	0x01

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Data Structures

#define __PACKED __attribute__ ((__packed__))

// BIOS Parameter Block
typedef struct {

	u16	bytesPerSector;
	u8	sectorsPerCluster;
	u16	reservedSectors;
	u8	numFATs;
	u16	rootEntries;
	u16	numSectorsSmall;
	u8	mediaDesc;
	u16	sectorsPerFAT;
	u16	sectorsPerTrk;
	u16	numHeads;
	u32	numHiddenSectors;
	u32	numSectors;
} __PACKED BIOS_BPB;

// Boot Sector - must be packed
typedef struct
{
	u8	jmpBoot[3];
	u8	OEMName[8];
	BIOS_BPB bpb;
	union	// Different types of extended BIOS Parameter Block for FAT16 and FAT32
	{
		struct  
		{
			// Ext BIOS Parameter Block for FAT16
			u8	driveNumber;
			u8	reserved1;
			u8	extBootSig;
			u32	volumeID;
			u8	volumeLabel[11];
			u8	fileSysType[8];
			// Bootcode
			u8	bootCode[448];
		}	__PACKED fat16;
		struct  
		{
			// FAT32 extended block
			u32	sectorsPerFAT32;
			u16	extFlags;
			u16	fsVer;
			u32	rootClus;
			u16	fsInfo;
			u16	bkBootSec;
			u8	reserved[12];
			// Ext BIOS Parameter Block for FAT16
			u8	driveNumber;
			u8	reserved1;
			u8	extBootSig;
			u32	volumeID;
			u8	volumeLabel[11];
			u8	fileSysType[8];
			// Bootcode
			u8	bootCode[420];
		}	__PACKED fat32;
	}	__PACKED extBlock;

	__PACKED	u16	bootSig;

}	__PACKED BOOT_SEC;


// Directory entry - must be packed
typedef struct
{
	u8	name[8];
	u8	ext[3];
	u8	attrib;
	u8	reserved;
	u8	cTime_ms;
	u16	cTime;
	u16	cDate;
	u16	aDate;
	u16	startClusterHigh;
	u16	mTime;
	u16	mDate;
	u16	startCluster;
	u32	fileSize;
}	__PACKED DIR_ENT;

// File information - no need to pack
typedef struct
{
	u32 firstCluster;
	u32 length;
	u32 curPos;
	u32 curClus;			// Current cluster to read from
	int curSect;			// Current sector within cluster
	int curByte;			// Current byte within sector
	char readBuffer[512];	// Buffer used for unaligned reads
	u32 appClus;			// Cluster to append to
	int appSect;			// Sector within cluster for appending
	int appByte;			// Byte within sector for appending
	bool read;	// Can read from file
	bool write;	// Can write to file
	bool append;// Can append to file
	bool inUse;	// This file is open
	u32 dirEntSector;	// The sector where the directory entry is stored
	int dirEntOffset;	// The offset within the directory sector
}	FAT_FILE;


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Global Variables

// _VARS_IN_RAM variables are stored in the largest section of WRAM 
// available: IWRAM on NDS ARM7, EWRAM on NDS ARM9 and GBA

// Locations on card
int discRootDir;
int discRootDirClus;
int discFAT;
int discSecPerFAT;
int discNumSec;
int discData;
int discBytePerSec;
int discSecPerClus;
int discBytePerClus;
u8 discBytePerClusShift;
u8 discSecPerClusShift;
#ifdef MORECLUSTERBUFFERS
int fatAccessCounter = 0;
int prevNextClust[nextClusterBufferCount] = {0};
#else
int prevNextClust = 0;
#endif
int prevFirstClust = -1;
int prevSect = -1;
int prevClust = -1;


enum {FS_UNKNOWN, FS_FAT12, FS_FAT16, FS_FAT32}
discFileSystem;

// Global sector buffer to save on stack space
unsigned char nextClusterBuffer[BYTES_PER_SECTOR];
unsigned char lastGlobalBuffer[BYTES_PER_SECTOR];
unsigned char globalBuffer[BYTES_PER_SECTOR];

#define CLUSTER_CACHE      0x2700000 // Main RAM
#define CLUSTER_CACHE_SIZE 0x80000 // 512K

u32* lastClusterCacheUsed = (u32*) CLUSTER_CACHE;
u32 clusterCache = CLUSTER_CACHE;
#ifndef B4DS
u32 currentClusterCacheSize = 0;
#endif
u32 clusterCacheSize = CLUSTER_CACHE_SIZE;


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//FAT routines

u32 FAT_ClustToSect (u32 cluster) {
	return (((cluster-2) * discSecPerClus) + discData);
}

#ifdef MORECLUSTERBUFFERS
int FAT_ReadNextClusterCache(u32 sector)
{
	int curSector = 0;
	for (int i = 0; i < nextClusterBufferCount; i++) {
		if (prevNextClust[i] == sector) {
			return i;
		}
	}

	curSector = fatAccessCounter;
	CARD_ReadSector(sector, nextClusterBuffer[curSector], 0, 0);
	prevNextClust[curSector] = sector;
	fatAccessCounter++;
	if (fatAccessCounter == nextClusterBufferCount) fatAccessCounter = 0;

	return curSector;
}
#endif

/*-----------------------------------------------------------------
FAT_NextCluster
Internal function - gets the cluster linked from input cluster
-----------------------------------------------------------------*/
u32 FAT_NextCluster(u32 cluster)
{
	u32 nextCluster = CLUSTER_FREE;
	u32 sector;
	int offset;
#ifdef MORECLUSTERBUFFERS
	int curSector;
#endif


	switch (discFileSystem) 
	{
		case FS_UNKNOWN:
			nextCluster = CLUSTER_FREE;
			break;

		case FS_FAT12:
			sector = discFAT + (((cluster * 3) / 2) / BYTES_PER_SECTOR);
			offset = ((cluster * 3) / 2) % BYTES_PER_SECTOR;
			#ifdef MORECLUSTERBUFFERS
			curSector = FAT_ReadNextClusterCache(sector);
			// read the nextCluster value
			nextCluster = ((u8*) nextClusterBuffer[curSector])[offset];
			#else
			if ((u32)prevNextClust != sector) {
				CARD_ReadSector(sector, nextClusterBuffer, 0, 0);
				prevNextClust = sector;
			}
			// read the nextCluster value
			nextCluster = ((u8*) nextClusterBuffer)[offset];
			#endif
			offset++;

			if (offset >= BYTES_PER_SECTOR) {
				offset = 0;
				sector++;
			}

			#ifdef MORECLUSTERBUFFERS
			curSector = FAT_ReadNextClusterCache(sector);
			// read the nextCluster value
			nextCluster |= (((u8*) nextClusterBuffer[curSector])[offset]) << 8;
			#else
			if ((u32)prevNextClust != sector) {
				CARD_ReadSector(sector, nextClusterBuffer, 0, 0);
				prevNextClust = sector;
			}
			// read the nextCluster value
			nextCluster |= (((u8*) nextClusterBuffer)[offset]) << 8;
			#endif

			if (cluster & 0x01) {
				nextCluster = nextCluster >> 4;
			} else 	{
				nextCluster &= 0x0FFF;
			}

			break;

		case FS_FAT16:
			sector = discFAT + ((cluster << 1) / BYTES_PER_SECTOR);
			offset = cluster % (BYTES_PER_SECTOR >> 1);

			#ifdef MORECLUSTERBUFFERS
			curSector = FAT_ReadNextClusterCache(sector);
			// read the nextCluster value
			nextCluster = ((u16*)nextClusterBuffer[curSector])[offset];
			#else
			if ((u32)prevNextClust != sector) {
				CARD_ReadSector(sector, nextClusterBuffer, 0, 0);
				prevNextClust = sector;
			}
			// read the nextCluster value
			nextCluster = ((u16*)nextClusterBuffer)[offset];
			#endif

			if (nextCluster >= 0xFFF7)
			{
				nextCluster = CLUSTER_EOF;
			}
			break;

		case FS_FAT32:
			sector = discFAT + ((cluster << 2) / BYTES_PER_SECTOR);
			offset = cluster % (BYTES_PER_SECTOR >> 2);

			#ifdef MORECLUSTERBUFFERS
			curSector = FAT_ReadNextClusterCache(sector);
			// read the nextCluster value
			nextCluster = (((u32*)nextClusterBuffer[curSector])[offset]) & 0x0FFFFFFF;
			#else
			if ((u32)prevNextClust != sector) {
				CARD_ReadSector(sector, nextClusterBuffer, 0, 0);
				prevNextClust = sector;
			}
			// read the nextCluster value
			nextCluster = (((u32*)nextClusterBuffer)[offset]) & 0x0FFFFFFF;
			#endif

			if (nextCluster >= 0x0FFFFFF7)
			{
				nextCluster = CLUSTER_EOF;
			}
			break;

		default:
			nextCluster = CLUSTER_FREE;
			break;
	}

	return nextCluster;
}

/*-----------------------------------------------------------------
ucase
Returns the uppercase version of the given char
char IN: a character
char return OUT: uppercase version of character
-----------------------------------------------------------------*/
char ucase (char character)
{
	if ((character > 0x60) && (character < 0x7B))
		character = character - 0x20;
	return (character);
}

__attribute__((target("arm")))
__attribute__ ((noinline))
static u32 ctz(u32 x)
{
	return __builtin_ctz(x);
}

/*-----------------------------------------------------------------
FAT_InitFiles
Reads the FAT information from the CF card.
You need to call this before reading any files.
bool return OUT: true if successful.
-----------------------------------------------------------------*/
bool FAT_InitFiles (bool initCard)
{
	int i;
	int bootSector;
	BOOT_SEC* bootSec;

	if (initCard && !CARD_StartUp())
	{
		#ifdef DEBUG
		nocashMessage("!CARD_StartUp()");
		#endif
		return (false);
	}

	// Read first sector of card
	if (!CARD_ReadSector (0, globalBuffer, 0, 0))
	{
		#ifdef DEBUG
		nocashMessage("!CARD_ReadSector (0, globalBuffer)");
		#endif
		return false;
	}
	// Check if there is a FAT string, which indicates this is a boot sector
	if ((globalBuffer[0x36] == 'F') && (globalBuffer[0x37] == 'A') && (globalBuffer[0x38] == 'T'))
	{
		bootSector = 0;
	}
	// Check for FAT32
	else if ((globalBuffer[0x52] == 'F') && (globalBuffer[0x53] == 'A') && (globalBuffer[0x54] == 'T'))
	{
		bootSector = 0;
	}
	else	// This is an MBR
	{
		// Find first valid partition from MBR
		// First check for an active partition
		for (i=0x1BE; (i < 0x1FE) && (globalBuffer[i] != 0x80); i+= 0x10);
		// If it didn't find an active partition, search for any valid partition
		if (i == 0x1FE) 
			for (i=0x1BE; (i < 0x1FE) && (globalBuffer[i+0x04] == 0x00); i+= 0x10);

		// Go to first valid partition
		if ( i != 0x1FE)	// Make sure it found a partition
		{
			bootSector = globalBuffer[0x8 + i] + (globalBuffer[0x9 + i] << 8) + (globalBuffer[0xA + i] << 16) + ((globalBuffer[0xB + i] << 24) & 0x0F);
		} else {
			bootSector = 0;	// No partition found, assume this is a MBR free disk
		}
	}

	// Read in boot sector
	bootSec = (BOOT_SEC*) globalBuffer;
	CARD_ReadSector (bootSector, bootSec, 0, 0);

	// Store required information about the file system
	if (bootSec->bpb.sectorsPerFAT != 0)
		discSecPerFAT = bootSec->bpb.sectorsPerFAT;
	else
		discSecPerFAT = bootSec->extBlock.fat32.sectorsPerFAT32;

	if (bootSec->bpb.numSectorsSmall != 0)
		discNumSec = bootSec->bpb.numSectorsSmall;
	else
		discNumSec = bootSec->bpb.numSectors;

	discBytePerSec = BYTES_PER_SECTOR;	// Sector size is redefined to be 512 bytes
	discSecPerClus = bootSec->bpb.sectorsPerCluster * bootSec->bpb.bytesPerSector / BYTES_PER_SECTOR;
	discBytePerClus = discBytePerSec * discSecPerClus;
	discBytePerClusShift = ctz(discBytePerClus);
	discSecPerClusShift = ctz(discSecPerClus);

	discFAT = bootSector + bootSec->bpb.reservedSectors;

	discRootDir = discFAT + (bootSec->bpb.numFATs * discSecPerFAT);
	discData = discRootDir + ((bootSec->bpb.rootEntries * sizeof(DIR_ENT)) / BYTES_PER_SECTOR);

	u32 bpbSectorsPerClusterShift = ctz(bootSec->bpb.sectorsPerCluster);

	if (((discNumSec - discData) >> bpbSectorsPerClusterShift) < 4085)
	{
		discFileSystem = FS_FAT12;
	}
	else if (((discNumSec - discData) >> bpbSectorsPerClusterShift) < 65525)
	{
		discFileSystem = FS_FAT16;
	}
	else
	{
		discFileSystem = FS_FAT32;
	}

	if (discFileSystem != FS_FAT32)
	{
		discRootDirClus = FAT16_ROOT_DIR_CLUSTER;
	}
	else	// Set up for the FAT32 way
	{
		discRootDirClus = bootSec->extBlock.fat32.rootClus;
		// Check if FAT mirroring is enabled
		if (!(bootSec->extBlock.fat32.extFlags & 0x80))
		{
			// Use the active FAT
			discFAT = discFAT + ( discSecPerFAT * (bootSec->extBlock.fat32.extFlags & 0x0F));
		}
	}

	#ifdef DEBUG
	nocashMessage("FAT_InitFiles OK");
	#endif

	return (true);
}


#ifndef NOGETBOOTCLUSTER
/*-----------------------------------------------------------------
getBootFileCluster
-----------------------------------------------------------------*/
void getBootFileCluster(aFile* file, const char* bootName)
{
	const DIR_ENT* dir = 0;
	int firstSector = 0;
	bool notFound = false;
	bool found = false;
//	int maxSectors;
	u32 wrkDirCluster = discRootDirClus;
	u32 wrkDirSector = 0;
	int wrkDirOffset = 0;
	int nameOffset;

	// Check if fat has been initialised
	if (discBytePerSec == 0)
	{
		#ifdef DEBUG
		nocashMessage("getBootFileCluster  fat not initialised");
		#endif

		file->firstCluster = CLUSTER_FREE;
		file->currentCluster = file->firstCluster;
		file->currentOffset = 0;
		return;
	}

	char *ptr = (char*)bootName;
	while (*ptr != '.') ptr++;
	int namelen = ptr - bootName;

//	maxSectors = (wrkDirCluster == FAT16_ROOT_DIR_CLUSTER ? (discData - discRootDir) : discSecPerClus);
	// Scan Dir for correct entry
	firstSector = discRootDir;
	prevSect = -1;
	CARD_ReadSector (firstSector + wrkDirSector, globalBuffer, 0, 0);
	found = false;
	notFound = false;
	wrkDirOffset = -1;	// Start at entry zero, Compensating for increment
	while (!found && !notFound) {
		wrkDirOffset++;
		if (wrkDirOffset == BYTES_PER_SECTOR / sizeof (DIR_ENT))
		{
			wrkDirOffset = 0;
			wrkDirSector++;
			if ((wrkDirSector == (u32)discSecPerClus) && (wrkDirCluster != FAT16_ROOT_DIR_CLUSTER))
			{
				wrkDirSector = 0;
				wrkDirCluster = FAT_NextCluster(wrkDirCluster);
				if (wrkDirCluster == CLUSTER_EOF)
				{
					notFound = true;
				}
				firstSector = FAT_ClustToSect(wrkDirCluster);
			}
			else if ((wrkDirCluster == (u32)FAT16_ROOT_DIR_CLUSTER) && (wrkDirSector == (u32)(discData - discRootDir)))
			{
				notFound = true;	// Got to end of root dir
			}
			CARD_ReadSector (firstSector + wrkDirSector, globalBuffer, 0, 0);
		}
		dir = &((DIR_ENT*) globalBuffer)[wrkDirOffset];
		found = true;
		if ((dir->attrib & ATTRIB_DIR) || (dir->attrib & ATTRIB_VOL))
		{
			found = false;
		}
		if(namelen<8 && dir->name[namelen]!=0x20) found = false;
		for (nameOffset = 0; nameOffset < namelen && found; nameOffset++)
		{
			if (ucase(dir->name[nameOffset]) != bootName[nameOffset])
				found = false;
		}
		for (nameOffset = 0; nameOffset < 3 && found; nameOffset++)
		{
			if (ucase(dir->ext[nameOffset]) != bootName[nameOffset+namelen+1])
				found = false;
		}
		if (dir->name[0] == FILE_LAST)
		{
			notFound = true;
		}
	} 

	// If no file is found, return CLUSTER_FREE
	if (notFound)
	{
		#ifdef DEBUG
		nocashMessage("getBootFileCluster  notFound");
		#endif

		file->firstCluster = CLUSTER_FREE;
		file->currentCluster = file->firstCluster;
		file->currentOffset = 0;
		file->fatTableCached = false;

		return;
	}

	#ifdef DEBUG
	nocashMessage("getBootFileCluster  found");
	#endif

	if (dir)
		file->firstCluster = (dir->startCluster | (dir->startClusterHigh << 16));
	else
		file->firstCluster = CLUSTER_FREE;
	
	file->currentCluster = file->firstCluster;
	file->currentOffset = 0;
	file->fatTableCached = false;
	return;
}
#endif

void getFileFromCluster(aFile* file, u32 cluster)
{
	file->firstCluster = cluster;
	file->currentCluster = file->firstCluster;
	file->currentOffset = 0;
	file->fatTableCached = false;
}

u32 getCachedCluster(aFile * file, int clusterIndex)
{
	if (file->fatTableCompressed) {
		const u32* fatCache = file->fatTableCache;
		u32 offset = 0;
		while (offset <= (u32)clusterIndex) {
			const u32 cluster = fatCache[0];
			if ((u32)clusterIndex >= offset && (u32)clusterIndex < offset + fatCache[1]) {
				return cluster + (clusterIndex - offset);
			}
			offset += fatCache[1];
			fatCache += 2;
		}
	}
	return file->fatTableCache[clusterIndex];
}

static u32 findCluster(aFile* file, u32 startOffset)
{
	u32 clusterIndex = 0;
	if (file->fatTableCached) {
    	#ifdef DEBUG
        nocashMessage("fat table cached");
        #endif
		clusterIndex = startOffset >> discBytePerClusShift;
		file->currentOffset=clusterIndex*discBytePerClus;
		file->currentCluster = getCachedCluster(file, clusterIndex);
	} else {
        #ifdef DEBUG
        nocashMessage("fatTable not cached");
        #endif
		if(startOffset<file->currentOffset) {
			file->currentOffset=0;
			file->currentCluster = file->firstCluster;
		}

		// Follow cluster list until desired one is found
		for (int chunks = (startOffset-file->currentOffset) >> discBytePerClusShift; chunks > 0; chunks--)
		{
			file->currentCluster = FAT_NextCluster (file->currentCluster);
			file->currentOffset+=discBytePerClus;
		}
	}
	return clusterIndex;
}

#ifndef B4DS
#ifndef _NO_SDMMC
static readContext context;

/*-----------------------------------------------------------------
fileReadNonBLocking(buffer, cluster, startOffset, length)
-----------------------------------------------------------------*/
bool fileReadNonBLocking (char* buffer, aFile * file, u32 startOffset, u32 length)
{
	#ifdef DEBUG
	nocashMessage("fileRead");
    dbg_hexa(buffer);   
    dbg_hexa(startOffset);
    dbg_hexa(length);
	#endif

	context.dataPos = 0;
    context.file = file;
    context.buffer = buffer;
    context.length = length;

	int beginBytes;

    context.clusterIndex = 0;

	if (file->firstCluster == CLUSTER_FREE || file->firstCluster == CLUSTER_EOF) {
		return true;
	}

	context.clusterIndex = findCluster(file, startOffset);

	// Calculate the sector and byte of the current position,
	// and store them
	context.curSect = (startOffset & (discBytePerClus - 1)) / BYTES_PER_SECTOR;
	context.curByte = startOffset % BYTES_PER_SECTOR;
    context.dataPos=0;
    beginBytes = (BYTES_PER_SECTOR < length + context.curByte ? (u32)(BYTES_PER_SECTOR - context.curByte) : length);

	// Load sector buffer for new position in file
	CARD_ReadSector( context.curSect + FAT_ClustToSect(file->currentCluster), buffer+context.dataPos, context.curByte, 0);
	context.curSect++;

    context.curByte+=beginBytes;
    context.dataPos+=beginBytes;

    context.chunks = ((int)length - beginBytes) / BYTES_PER_SECTOR;
    context.cmd=0;

    return false;
}

bool resumeFileRead()
{
    if(context.cmd == 0 || CARD_CheckCommand(context.cmd))
    {
        if(context.chunks>0)
        {
    		int sectorsToRead=0;

    		if(context.file->fatTableCached) {
              // Move to the next cluster if necessary
              if (context.curSect >= discSecPerClus)
    			{
				  context.clusterIndex += context.curSect >> discSecPerClusShift;
				  context.curSect = context.curSect & (discSecPerClus - 1);
				  context.file->currentCluster = getCachedCluster(context.file, context.clusterIndex);
				  context.file->currentOffset += discBytePerClus;
    			}
               // Calculate how many sectors to read (try to group several cluster at a time if there is no fragmentation)
              for(int tempClusterIndex=context.clusterIndex; sectorsToRead<=context.chunks; ) {   
                  if(getCachedCluster(context.file, tempClusterIndex)+1 == getCachedCluster(context.file, tempClusterIndex+1)) {
                      #ifdef DEBUG
                  	nocashMessage("contiguous read");
                  	#endif
                      // the 2 cluster are consecutive
                      sectorsToRead += discSecPerClus;
                      tempClusterIndex++;    
                  } else {
                      #ifdef DEBUG
                  	nocashMessage("non contiguous read");
                  	#endif
                      break;
                  }
              }

              if(!sectorsToRead) sectorsToRead = discSecPerClus - context.curSect;
              else sectorsToRead = sectorsToRead - context.curSect;

              if(context.chunks < sectorsToRead) {
  		    sectorsToRead = context.chunks;
              }

              // Read the sectors
   			CARD_ReadSectorsNonBlocking(context.curSect + FAT_ClustToSect(context.file->currentCluster), sectorsToRead, context.buffer + context.dataPos);
   			context.chunks  -= sectorsToRead;
   			context.curSect += sectorsToRead;
   			context.dataPos += BYTES_PER_SECTOR * sectorsToRead;
              context.clusterIndex += context.curSect >> discSecPerClusShift;
              context.curSect = context.curSect & (discSecPerClus - 1);
			  context.file->currentCluster = getCachedCluster(context.file, context.clusterIndex);
              context.cmd=0x33C12;
              return false;         
          } else {
              // Move to the next cluster if necessary
  			if (context.curSect >= discSecPerClus)
  			{
  				context.curSect = 0;
                  context.file->currentCluster = FAT_NextCluster (context.file->currentCluster);
  				context.file->currentOffset+=discBytePerClus;
  			}

              // Calculate how many sectors to read (read a maximum of discSecPerClus at a time)
     	    sectorsToRead = discSecPerClus - context.curSect;
      	    if(context.chunks < sectorsToRead)
      		sectorsToRead = context.chunks;

  			CARD_ReadSectorsNonBlocking(context.curSect + FAT_ClustToSect(context.file->currentCluster), sectorsToRead, context.buffer + context.dataPos);
  			context.chunks  -= sectorsToRead;
  			context.curSect += sectorsToRead;
  			context.dataPos += BYTES_PER_SECTOR * sectorsToRead;
            context.cmd=0x33C12;
            return false;         
          }			
  	}
      else
      {
          // Take care of any bytes left over before end of read
      	if ((u32)context.dataPos < context.length)
      	{
              #ifdef DEBUG
            	nocashMessage("non aligned read, data is missing");
              if(context.length-context.dataPos>BYTES_PER_SECTOR) {
                  nocashMessage("error: unread sector are missing");
              }
              #endif

      		// Update the read buffer
      		context.curByte = 0;
      		if (context.curSect >= discSecPerClus)
      		{
      			if(context.file->fatTableCached) {
                        context.clusterIndex+= context.curSect >> discSecPerClusShift;
                        context.curSect = context.curSect & (discSecPerClus - 1);
						context.file->currentCluster = getCachedCluster(context.file, context.clusterIndex);
                    } else {
                        context.curSect = 0;
                        context.file->currentCluster = FAT_NextCluster (context.file->currentCluster);
                    }
      			context.file->currentOffset+=discBytePerClus;
      		}

            CARD_ReadSector( context.curSect + FAT_ClustToSect(context.file->currentCluster), lastGlobalBuffer, 0, 0);    

      		// Read in last partial chunk
              tonccpy(context.buffer+context.dataPos,lastGlobalBuffer+context.curByte,context.length-context.dataPos);

              context.curByte+=context.length;
              context.dataPos+=context.length;
      	}
          return true;
      }

    }  
	return false;
}
#endif
#endif

// Load sector buffer for new position in file
static void loadSectorBuf(aFile* file, int curSect)
{
	if ((u32)prevFirstClust != (u32)file->firstCluster || prevSect != curSect || (u32)prevClust != file->currentCluster) {
		prevFirstClust = file->firstCluster;
		CARD_ReadSector( curSect + FAT_ClustToSect(file->currentCluster), globalBuffer, 0, 0);
		prevSect = curSect;
		prevClust = file->currentCluster;
	}
}

void resetPrevSect()
{
	prevSect = -1;
}

/*-----------------------------------------------------------------
fileRead(buffer, cluster, startOffset, length)
-----------------------------------------------------------------*/
u32 fileRead (char* buffer, aFile* file, u32 startOffset, u32 length)
{
	#ifdef DEBUG
	nocashMessage("fileRead");
    dbg_hexa(buffer);   
    dbg_hexa(startOffset);
    dbg_hexa(length);
	#endif

	int curByte;
	int curSect;

	int dataPos = 0;
	int chunks;
	int beginBytes;
    
    u32 clusterIndex = 0;

	if (file->firstCluster == CLUSTER_FREE || file->firstCluster == CLUSTER_EOF) 
	{
		return 0;
	}

	if (file->fatTableCached && (file->fatTableCache[0] == CLUSTER_FREE || file->fatTableCache[0] == CLUSTER_EOF)) {
		// Cluster in FAT table cache is invalid
		return 0;
	}

	clusterIndex = findCluster(file, startOffset);

	// Calculate the sector and byte of the current position,
	// and store them
	curSect = (startOffset & (discBytePerClus - 1)) / BYTES_PER_SECTOR;
	curByte = startOffset % BYTES_PER_SECTOR;

	loadSectorBuf(file, curSect);

	curSect++;

	// Number of bytes needed to read to align with a sector
	beginBytes = (BYTES_PER_SECTOR < length + curByte ? (u32)(BYTES_PER_SECTOR - curByte) : length);

	// Read first part from buffer, to align with sector boundary
    dataPos=0;
    tonccpy(buffer,globalBuffer+curByte,beginBytes);
    curByte+=beginBytes;
    dataPos+=beginBytes;

	// Read in all the 512 byte chunks of the file directly, saving time
	for ( chunks = ((int)length - beginBytes) / BYTES_PER_SECTOR; chunks > 0;)
	{
		int sectorsToRead=0;

		if(file->fatTableCached) {
          
              // Move to the next cluster if necessary
              if (curSect >= discSecPerClus)
  			{
                  clusterIndex+= curSect >> discSecPerClusShift;
                  curSect &= (discSecPerClus - 1);
  				file->currentOffset+=discBytePerClus;
				file->currentCluster = getCachedCluster(file, clusterIndex);
  			}

               // Calculate how many sectors to read (try to group several cluster at a time if there is no fragmentation)
              for(int tempClusterIndex=clusterIndex; sectorsToRead<=chunks; ) {   
                  if(getCachedCluster(file, tempClusterIndex)+1 == getCachedCluster(file, tempClusterIndex+1)) {
                      #ifdef DEBUG
                  	nocashMessage("contiguous read");
                  	#endif
                      // the 2 cluster are consecutive
                      sectorsToRead += discSecPerClus;
                      tempClusterIndex++;    
                  } else {
                      #ifdef DEBUG
                  	nocashMessage("non contiguous read");
                  	#endif
                      break;
                  }
              }

              if(!sectorsToRead) sectorsToRead = discSecPerClus - curSect;
              else sectorsToRead = sectorsToRead - curSect;
              
              if(chunks < sectorsToRead) {
			    sectorsToRead = chunks;
              }

              // Read the sectors
    			CARD_ReadSectors(curSect + FAT_ClustToSect(file->currentCluster), sectorsToRead, buffer + dataPos);
    			chunks  -= sectorsToRead;
    			curSect += sectorsToRead;
    			dataPos += BYTES_PER_SECTOR * sectorsToRead;

              clusterIndex+= curSect >> discSecPerClusShift;
              curSect &= (discSecPerClus - 1);
				file->currentCluster = getCachedCluster(file, clusterIndex);
          } else {
              // Move to the next cluster if necessary
  			if (curSect >= discSecPerClus)
  			{
  				curSect = 0;
                  file->currentCluster = FAT_NextCluster (file->currentCluster);
  				file->currentOffset+=discBytePerClus;
  			}

              // Calculate how many sectors to read (read a maximum of discSecPerClus at a time)
		    sectorsToRead = discSecPerClus - curSect;
		    if(chunks < sectorsToRead)
			sectorsToRead = chunks;

              // Read the sectors
  			CARD_ReadSectors(curSect + FAT_ClustToSect(file->currentCluster), sectorsToRead, buffer + dataPos);
  			chunks  -= sectorsToRead;
  			curSect += sectorsToRead;
  			dataPos += BYTES_PER_SECTOR * sectorsToRead;
          }			
	}

	// Take care of any bytes left over before end of read
	if ((u32)dataPos < length)
	{
          #ifdef DEBUG
        	nocashMessage("non aligned read, data is missing");
          if(length-dataPos>BYTES_PER_SECTOR) {
              nocashMessage("error: unread sector are missing");
          }
          #endif

		// Update the read buffer
		if (curSect >= discSecPerClus)
		{
			if(file->fatTableCached) {
                  clusterIndex+= curSect >> discSecPerClusShift;
                  curSect &= (discSecPerClus - 1);
				file->currentCluster = getCachedCluster(file, clusterIndex);
              } else {
                  curSect = 0;
                  file->currentCluster = FAT_NextCluster (file->currentCluster);
              }
			file->currentOffset+=discBytePerClus;
		}

		CARD_ReadSector( curSect + FAT_ClustToSect(file->currentCluster), lastGlobalBuffer, 0, 0);

		// Read in last partial chunk
          tonccpy(buffer+dataPos,lastGlobalBuffer,length-dataPos);
          curByte+=length;
          dataPos+=length;
	}

      #ifdef DEBUG
      nocashMessage("fileRead completed");
      nocashMessage("");
      #endif

	return dataPos;
	
}

/*-----------------------------------------------------------------
fileWrite(buffer, cluster, startOffset, length)
-----------------------------------------------------------------*/
u32 fileWrite (const char* buffer, aFile* file, u32 startOffset, u32 length)
{
	#ifdef DEBUG
	nocashMessage("fileWrite");
	#endif

	int curByte;
	int curSect;

	int dataPos = 0;
	int chunks;
	int beginBytes;
    u32 clusterIndex = 0;

	if (file->firstCluster == CLUSTER_FREE || file->firstCluster == CLUSTER_EOF) 
	{
		#ifdef DEBUG
		nocashMessage("CLUSTER_FREE or CLUSTER_EOF");
		#endif
		return 0;
	}

	if (file->fatTableCached && (file->fatTableCache[0] == CLUSTER_FREE || file->fatTableCache[0] == CLUSTER_EOF)) {
		// Cluster in FAT table cache is invalid
		return 0;
	}

	clusterIndex = findCluster(file, startOffset);

	// Calculate the sector and byte of the current position,
	// and store them
	curSect = (startOffset & (discBytePerClus - 1)) / BYTES_PER_SECTOR;
	curByte = startOffset % BYTES_PER_SECTOR;

	loadSectorBuf(file, curSect);

	// Number of bytes needed to read to align with a sector
	beginBytes = (BYTES_PER_SECTOR < length + curByte ? (u32)(BYTES_PER_SECTOR - curByte) : length);

	// Read first part from buffer, to align with sector boundary
    dataPos=0;
    tonccpy(globalBuffer+curByte,buffer,beginBytes);
    curByte+=beginBytes;
    dataPos+=beginBytes;

	CARD_WriteSector(curSect + FAT_ClustToSect(file->currentCluster), globalBuffer);

	curSect++;

	// Read in all the 512 byte chunks of the file directly, saving time
	for ( chunks = ((int)length - beginBytes) / BYTES_PER_SECTOR; chunks > 0;)
	{
		int sectorsToWrite;

		// Move to the next cluster if necessary
		if (curSect >= discSecPerClus)
		{
            if(file->fatTableCached) {
                clusterIndex++;
                file->currentCluster = getCachedCluster(file, clusterIndex); 
            } else {
                file->currentCluster = FAT_NextCluster (file->currentCluster);
			    
            }
            file->currentOffset+=discBytePerClus;
			curSect = 0;
		}

		// Calculate how many sectors to read (read a maximum of discSecPerClus at a time)
		sectorsToWrite = discSecPerClus - curSect;
		if(chunks < sectorsToWrite)
			sectorsToWrite = chunks;

		// Read the sectors
		CARD_WriteSectors(curSect + FAT_ClustToSect(file->currentCluster), sectorsToWrite, buffer + dataPos);

		chunks  -= sectorsToWrite;
		curSect += sectorsToWrite;
		dataPos += BYTES_PER_SECTOR * sectorsToWrite;
	}

	// Take care of any bytes left over before end of read
	if ((u32)dataPos < length)
	{
		// Update the read buffer
		curByte = 0;
		if (curSect >= discSecPerClus)
		{
            if(file->fatTableCached) {
                clusterIndex++;
                file->currentCluster = getCachedCluster(file, clusterIndex);
            } else {
                file->currentCluster = FAT_NextCluster (file->currentCluster);
            }
			curSect = 0;
			file->currentOffset+=discBytePerClus;
		}
		CARD_ReadSector( curSect + FAT_ClustToSect(file->currentCluster), lastGlobalBuffer, 0, 0);

		// Read in last partial chunk
        tonccpy(lastGlobalBuffer+curByte,buffer+dataPos,length-dataPos);
        curByte+=length;
        dataPos+=length;

		CARD_WriteSector( curSect + FAT_ClustToSect(file->currentCluster), lastGlobalBuffer);
	}

	return dataPos;
}

void buildFatTableCache (aFile * file)
{
	if (file->fatTableCached) return;

    #ifdef DEBUG
	nocashMessage("buildFatTableCache");
    #endif
    
	file->currentOffset=0;
	file->currentCluster = file->firstCluster;

	file->fatTableCache = lastClusterCacheUsed;

	// Follow cluster list until desired one is found
	while (file->currentCluster != CLUSTER_EOF && file->firstCluster != CLUSTER_FREE 
		&& (u32)lastClusterCacheUsed<clusterCache+clusterCacheSize)
	{
		*lastClusterCacheUsed = file->currentCluster;
		file->currentOffset+=discBytePerClus;
		file->currentCluster = FAT_NextCluster (file->currentCluster);
		lastClusterCacheUsed++;
#ifndef B4DS
		currentClusterCacheSize += 4;
#endif
		file->fatTableCacheSize += 4;
	}

	if(file->currentCluster == CLUSTER_EOF) {
        #ifdef DEBUG
        nocashMessage("fat table cached");
        #endif
		file->fatTableCached = true;
	}
    #ifdef DEBUG 
    else {
      nocashMessage("fat table not cached");
	  file->fatTableCacheSize = 0;
	  lastClusterCacheUsed = lastClusterCacheUsedBak;
    }
    #endif

	file->currentOffset=0;
	file->currentCluster = file->firstCluster;
}

void buildFatTableCacheCompressed (aFile * file)
{
	if (file->fatTableCached) return;

    #ifdef DEBUG
	nocashMessage("buildFatTableCacheCompressed");
    #endif
    
	file->currentOffset=0;
	file->currentCluster = file->firstCluster;

	file->fatTableCache = lastClusterCacheUsed;

	// Follow cluster list until desired one is found
	while (file->currentCluster != CLUSTER_EOF && file->firstCluster != CLUSTER_FREE 
		&& (u32)lastClusterCacheUsed<clusterCache+clusterCacheSize)
	{
		u32 clusterNext = file->currentCluster;
		int clusterCount = 1; // Adjacent cluster count

		*lastClusterCacheUsed = file->currentCluster;
		while (file->currentCluster != CLUSTER_EOF && file->firstCluster != CLUSTER_FREE) {
			file->currentOffset+=discBytePerClus;
			file->currentCluster = FAT_NextCluster (file->currentCluster);
			clusterNext++;
			if (file->currentCluster == clusterNext) {
				clusterCount++;
			} else {
				break;
			}
		}
		lastClusterCacheUsed++;
		#ifndef B4DS
		currentClusterCacheSize += 4;
		#endif
		file->fatTableCacheSize += 4;

		*lastClusterCacheUsed = clusterCount;
		lastClusterCacheUsed++;
		#ifndef B4DS
		currentClusterCacheSize += 4;
		#endif
		file->fatTableCacheSize += 4;
	}

	if(file->currentCluster == CLUSTER_EOF) {
        #ifdef DEBUG
        nocashMessage("fat table cached and compressed");
        #endif
		file->fatTableCached = true;
		file->fatTableCompressed = true;
	}
    #ifdef DEBUG 
    else {
      nocashMessage("fat table not cached and compressed");
	  file->fatTableCacheSize = 0;
	  lastClusterCacheUsed = lastClusterCacheUsedBak;
    }
    #endif

	file->currentOffset=0;
	file->currentCluster = file->firstCluster;
}