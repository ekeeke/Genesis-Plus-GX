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
* Diskio Module
* -------------
*
* This module is almost identical to the one found in ChaN's TinyFAT FS.
* It's a logical abstration after all :)
*
* This covers stdio file on a SD image file
****************************************************************************/
#ifndef __DISKIO__
#define __DISKIO__

int DISKIO_Init( int drive );
int DISKIO_ReadSectors( int drive, void *buffer, int sector, int count );

#endif

