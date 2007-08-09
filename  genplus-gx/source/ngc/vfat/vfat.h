/****************************************************************************
* FAT16 - VFAT Support
*
* NOTE: Only supports FAT16 with Long File Names
*       I have no interest in adding FAT32
*
* Reference Documentation:
*
*	FAT: General Overview of On-Disk Format
*	Version 1.02 May 05, 1999
*	Microsoft Corporation
*
*       FAT: General Overview of On-Disk Format
*	Version 1.03 December 06, 2000
*	Microsoft Corporation
*
* This is targetted at MMC/SD cards.
*
* Copyright softdev 2007. All rights reserved.
*
* $Date: 2007-08-03 13:23:19 +0100 (Fri, 03 Aug 2007) $
* $Rev: 3 $
****************************************************************************/
#ifndef __FATVFAT__
#define __FATVFAT__

/* x86 type definitions */
typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;

/* Big Endian Support */
#ifdef WORDS_BIGENDIAN
#define SWAP16(a) (((a&0xff)<<8) | ((a&0xff00)>>8))
#define SWAP32(a) (((a&0xff000000)>>24) | ((a&0xff0000) >> 8) | ((a&0xff00)<<8) |((a&0xff)<<24))
#else
#define SWAP16(a) (a)
#define SWAP32(a) (a)
#endif

/* General */
#define SECTOR_SIZE		512
#define SECTOR_SHIFT_BITS	9
#define LFN_LAST_ENTRY		0x40
#define LFN_ENTRY_MASK		0x3F
#define ROOTCLUSTER		0xdeadc0de
#define PSEP			'/'
#define PSEPS			"/"
#define DIR_ROOT		"."
#define DIR_PARENT		".."

/* FSTYPES */
#define FS_TYPE_NONE	0
#define FS_TYPE_FAT16	1

/* Errors */
#define FS_FILE_OK	0
#define FS_SUCCESS	FS_FILE_OK
#define FS_ERR_NOMEM	-128
#define FS_NO_FILE	-64
#define FS_ERR_IO	-32
#define FS_ERR_PARAM	-16

/* File modes */
#define FS_READ		1

/* Gamecube Specific */
#define FS_SLOTA	0
#define FS_SLOTB	1

/* FAT12/16 */
typedef struct
  {
    BYTE jmpBoot[3];	/* Always 0xEBxx90 or 0xE9xxxx */
    BYTE OEMName[8];	/* OEM Name 'MSWIN4.1' or similar */
    WORD bytesPerSec;	/* Bytes per sector */
    BYTE secPerClust;	/* Sectors per cluster */
    WORD reservedSec;	/* Reserved Sector Count */
    BYTE numFATs;	/* Number of FAT copies */
    WORD rootEntCount;	/* FAT12/16 number of root entries. */
    WORD totSec16;	/* Sector count if < 0x10000 */
    BYTE media;		/* Media ID byte (HD == 0xF8) */
    WORD FATsz16;	/* Sectors occupied by one copy of FAT */
    WORD secPerTrack;	/* Sectors per track */
    WORD numHeads;	/* Number of heads */
    DWORD hiddenSec;	/* Hidden sector count */
    DWORD totSec32;	/* Total sectors when >= 0x10000 */
    BYTE drvNum;	/* BIOS Drive Number (0x80) */
    BYTE reserved1;	/* Unused - always zero */
    BYTE bootSig;	/* Boot signature */
    DWORD volID;	/* Volume serial number */
    BYTE volName[11];	/* Volume Name */
    BYTE FilSysType[8];	/* File system type */
    BYTE filler[SECTOR_SIZE-64];
    BYTE sigkey1;	/* 0x55 */
    BYTE sigkey2;	/* 0xAA */
  }
__attribute__((__packed__)) BPB16;

/* Partition entry */
typedef struct
  {
    BYTE bootindicator;
    BYTE startCHS[3];
    BYTE partitiontype;
    BYTE endCHS[3];
    DWORD partitionstart;
    DWORD partitionsize;
  }
__attribute__((__packed__)) PARTENTRY;

/* VFAT - Main structure */
typedef struct
  {
    DWORD	BaseOffset;
    DWORD	SectorsPerCluster;
    DWORD	BytesPerSector;
    DWORD	ReservedSectors;
    DWORD	RootDirSectors;
    DWORD	SectorsPerFAT;
    DWORD	NumberOfFATs;
    DWORD	FirstDataSector;
    DWORD	TotalSectors;
    DWORD	CountOfClusters;
    DWORD	DataSectors;
    DWORD	RootDirOffset;
    DWORD	FirstFATOffset;
    DWORD	RootDirEntries;
    WORD	*FAT;		/* Holds first FAT copy */
    BYTE	*rootDir;	/* Holds entire root directory */
  }
__attribute__((__packed__)) VFATFS;

/**
 * Directory
 */

#define MAX_LONG_NAME		256

/* Directory entry attributes */
#define ATTR_READ_ONLY		0x01
#define ATTR_HIDDEN		0x02
#define ATTR_SYSTEM		0x04
#define ATTR_VOLUME_ID		0x08
#define ATTR_DIRECTORY		0x10
#define ATTR_ARCHIVE		0x20
#define ATTR_LONG_NAME		(ATTR_READ_ONLY | \
				 ATTR_HIDDEN | \
				 ATTR_SYSTEM | \
				 ATTR_VOLUME_ID )

#define ATTR_LONG_NAME_MASK	( ATTR_READ_ONLY | \
				  ATTR_HIDDEN | \
				  ATTR_SYSTEM | \
				  ATTR_VOLUME_ID | \
				  ATTR_DIRECTORY | \
				  ATTR_ARCHIVE )

#define CLUSTER_END_CHAIN	  0xFFF8
#define CLUSTER_BAD		  0xFFF7

/* Short file name */
typedef struct
  {
    BYTE dirname[11];	/* Record name */
    BYTE attribute;	/* Attributes */
    BYTE NTReserved;	/* Reserved for Window NT - set 0 */
    BYTE dirTenthSecs;	/* Tenth of a second, 0-199 */
    WORD dirCreateTime;	/* Time of creation */
    WORD dirCreateDate;	/* Date of creation */
    WORD dirLastAccDate;/* Date of last access */
    WORD fstClustHigh;	/* High word of first cluster - ZERO on FAT16 */
    WORD dirWriteTime;	/* Time of last write */
    WORD dirWriteDate;	/* Date of last write */
    WORD fstClustLow;	/* Low word of first cluster */
    DWORD filesize;	/* Filesize in bytes */
  }
__attribute__((__packed__)) SFNDIRREC;

/* Long file name */
typedef struct
  {
    BYTE ordinal;	/* Entry number */
    BYTE dirname1[10];
    BYTE attribute;	/* Attributes */
    BYTE type;		/* Reserved */
    BYTE checksum;	/* SFN Checksum */
    BYTE dirname2[12];
    WORD fstClustLo;	/* MUST BE ZERO */
    BYTE dirname3[4];
  }
__attribute__((__packed__)) LFNDIRREC;

/* User dir entry */
typedef struct
  {
    BYTE	longname[MAX_LONG_NAME];
    BYTE	shortname[13];	/* Keep word aligned*/
    DWORD	fpos;
    DWORD	fsize;
    DWORD	driveid;
    DWORD	FirstCluster;
    DWORD	CurrentCluster;
    DWORD	CachedCluster;
    DWORD	CurrentDirEntry;
    DWORD	crosscluster;
    BYTE	*clusterdata;
    /* Now a copy of the current directory entry */
    SFNDIRREC dirent;
  }
__attribute__((__packed__)) FSDIRENTRY;

/* VFAT API */
/* Directory */
int VFAT_opendir( int drive, FSDIRENTRY *d, char *search );
int VFAT_readdir( FSDIRENTRY *d );
void VFAT_closedir( FSDIRENTRY *d );
int VFAT_fopen( int drive, FSDIRENTRY *d, char *fname, int mode );
void VFAT_fclose( FSDIRENTRY *d );
int VFAT_fread( FSDIRENTRY *d, void *buffer, int length );
int VFAT_ftell( FSDIRENTRY *d );
int VFAT_fseek( FSDIRENTRY *d, int where, int whence );
int VFAT_mount( int driveid, VFATFS *v );

#endif

