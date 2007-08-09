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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "vfat.h"
#include "diskio.h"

static BYTE sector[SECTOR_SIZE];	/* Local sector buffer */
static VFATFS *vfs[2];			/* VFAT Pointers for 2 drives */

#ifdef WORDS_BIGENDIAN
#define strcasecmp stricmp
#endif

/**
 * Z E R O   S E C T O R  /  B I O S   P A R A M E T E R   B L O C K
 *
 * These functions take care of parsing the 0th sector/BPB
 * Supports SuperFloppy Format and standard partitioning.
 * 
 */

static int BPBCheck( BYTE *sector )
{
  BPB16 *bpb = (BPB16 *)sector;

  /* Check signatures */
  if ( ( bpb->sigkey1 == 0x55 ) && ( bpb->sigkey2 == 0xAA ) )
    {
      /* Check for FAT16 signature */
      if ( memcmp(bpb->FilSysType, "FAT16", 5) == 0 )
        return FS_TYPE_FAT16;
      /* Non MS utilities simply put FAT */
      if ( memcmp(bpb->FilSysType, "FAT", 3) == 0 )
        return FS_TYPE_FAT16;
    }

  return FS_TYPE_NONE;
}

static int PECheck( BYTE *sector )
{
  int i;
  PARTENTRY *pe;

  if ( ( sector[SECTOR_SIZE-2] == 0x55 ) && ( sector[SECTOR_SIZE-1] == 0xAA ) )
    {
      /* Find a FAT16 partition entry */
      for( i = 0; i < 4; i++ )
        {
          pe = (PARTENTRY *)(sector + 446 + (i<<4));
          if ( pe->partitiontype == 0x06 )
            {
              return SWAP32(pe->partitionstart);
            }
        }
    }

  return FS_TYPE_NONE;
}

/****************************************************************************
* VFAT_Mount
*
* Function to mount a FAT16-VFAT volume
***************************************************************************/
int VFAT_mount( int driveid, VFATFS *v )
{
  int ret;
  int bpbsector = 0;
  BPB16 *bpb = (BPB16 *)sector;
  BYTE media = 0;

  if ( driveid < 0 || driveid > 1 )
    return FS_TYPE_NONE;

  memset(v, 0, sizeof(VFATFS));

  /* Copy pointer */
  vfs[driveid] = v;

  if ( DISKIO_Init( driveid ) != FS_SUCCESS )
    return FS_ERR_IO;

  if ( DISKIO_ReadSectors( driveid, sector, 0, 1 ) != FS_SUCCESS )
    return FS_ERR_IO;

  /* Check for SuperFloppy Format */
  ret = BPBCheck( sector );

  if ( ret == FS_TYPE_NONE )
    {
      /* Check for Partition Entry */
      bpbsector = PECheck(sector);
      if ( !bpbsector )
        return FS_TYPE_NONE;

      if ( DISKIO_ReadSectors( driveid, sector, bpbsector, 1 ) != FS_SUCCESS )
        return FS_ERR_IO;

      /* Check BPB */
      ret = BPBCheck( sector );
    }

  if ( ret == FS_TYPE_FAT16 )
    {
      /* Capture defaults to machine native format */
      v->BaseOffset = bpbsector;
      v->BytesPerSector = SWAP16(bpb->bytesPerSec);
      v->SectorsPerFAT = SWAP16(bpb->FATsz16);
      v->ReservedSectors = SWAP16(bpb->reservedSec);
      v->NumberOfFATs = bpb->numFATs;
      v->SectorsPerCluster = bpb->secPerClust;
      v->RootDirEntries = SWAP16(bpb->rootEntCount);

      /* Calculate number of root directory sectors */
      v->RootDirSectors = ( ( SWAP16(bpb->rootEntCount) << 5 ) + ( v->BytesPerSector - 1 ) ) / v->BytesPerSector;

      /* First data sector */
      v->FirstDataSector = v->ReservedSectors + (v->NumberOfFATs * v->SectorsPerFAT) + v->RootDirSectors + v->BaseOffset;

      /* Total sectors */
      if ( bpb->totSec16 == 0 )
        v->TotalSectors = SWAP32(bpb->totSec32);
      else
        v->TotalSectors = SWAP16(bpb->totSec16);

      /* Data Sectors */
      v->DataSectors = v->TotalSectors - ( v->ReservedSectors + ( v->NumberOfFATs * v->SectorsPerFAT ) + v->RootDirSectors );

      /* Count of clusters */
      v->CountOfClusters = v->DataSectors / bpb->secPerClust;

      /* From v1.03 Document - Page 14 - FAT Type Determination */
      if ( v->CountOfClusters < 4085 )
        return FS_TYPE_NONE;	/* FAT12 Volume */
      else
        {
          if ( v->CountOfClusters >= 65525 )
            return FS_TYPE_NONE;	/* FAT32 Volume */
        }

      /* Root Directory Offset */
      v->RootDirOffset = v->ReservedSectors + ( bpb->numFATs * v->SectorsPerFAT ) + v->BaseOffset;

      /* First copy of FAT offset */
      v->FirstFATOffset = v->ReservedSectors + v->BaseOffset;

      media = bpb->media;

      /* Read first FAT */
      if ( DISKIO_ReadSectors( driveid, sector, v->FirstFATOffset, 1 ) != FS_SUCCESS )
        return FS_ERR_IO;

      if ( sector[0] == media )
        {
          /* Allocate work spaces */
          v->FAT = (WORD *)malloc(v->SectorsPerFAT * SECTOR_SIZE);
          if ( v->FAT == NULL )
            return FS_ERR_NOMEM;

          /* Save time running in and out - just load up the FAT table */
          if ( DISKIO_ReadSectors(driveid, v->FAT, v->FirstFATOffset, v->SectorsPerFAT) != FS_SUCCESS )
            {
              free(v->FAT);
              return FS_ERR_IO;
            }

          /* Likewise, the same for the root directory */
          v->rootDir = (BYTE *)malloc(v->BytesPerSector * v->RootDirSectors);
          if ( v->rootDir == NULL )
            {
              free(v->FAT);
              return FS_ERR_NOMEM;
            }

          /* Read root directory */
          if ( DISKIO_ReadSectors(driveid, v->rootDir, v->RootDirOffset, v->RootDirSectors) != FS_SUCCESS )
            {
              free(v->FAT);
              free(v->rootDir);
              return FS_ERR_IO;
            }
          return FS_TYPE_FAT16;
        }
    }

  return FS_TYPE_NONE;

}

/**
 * F I L E    N A M I N G   S U P P O R T
 *
 * Routines to en/decode long and short file names
 */

/****************************************************************************
* CalcShortNameChecksum
*
* Calculate the checksum for a short filename
* Filename should be in UPPER case, and padded with spaces to match
* a standard directory entry
****************************************************************************/
static unsigned char CalcShortNameCheckSum( BYTE *fname )
{
  int i;
  unsigned char sum = 0;

  for( i = 0; i < 11; i++ )
    sum = ( ( sum & 1 ) ? 0x80 : 0 ) + ( sum >> 1 ) + fname[i];

  return sum;
}

/****************************************************************************
* BuildShortNameFromDirEntry
*
* User friendly shortname
****************************************************************************/
static void BuildShortNameFromDirEntry( SFNDIRREC *sfn, BYTE *out )
{
  int i,j;

  for(i = 0, j = 0; i < 11; i++ )
    {
      if ( sfn->dirname[i] != 32 )
        {
          out[j++] = sfn->dirname[i];
        }

      if ( (i == 7) && ( sfn->dirname[8] != 32 ) )
        out[j++] = '.';
    }
}

/****************************************************************************
* BuildLongNameFromDirEntry
*
* Build a long name from unicode to asciiz.
* Each directory entry may contain up to 13 characters for sub entry.
****************************************************************************/
static void BuildLongNameFromDirEntry( LFNDIRREC *lfn, int position, BYTE *out )
{
  int j = ( ( position - 1 ) * 13 );
  int i;

  /* Part one */
  for( i = 0; i < 10; i += 2 )
    {
      if ( lfn->dirname1[i] == 0xFF )
        return;

      out[j++] = lfn->dirname1[i];
    }

  /* Part two */
  for( i = 0; i < 12; i += 2 )
    {
      if ( lfn->dirname2[i] == 0xFF )
        return;

      out[j++] = lfn->dirname2[i];
    }

  /* Part three */
  for( i = 0; i < 4; i += 2 )
    {
      if ( lfn->dirname3[i] == 0xFF )
        return;

      out[j++] = lfn->dirname3[i];
    }
}

/**
 * D I R E C T O R Y   F U N C T I O N S
 *
 * These routines take care of all directory level parsing
 */

static int SectorFromCluster( int drive, int cluster )
{
  VFATFS *v = vfs[drive];
  return ( ( cluster - 2 ) * v->SectorsPerCluster ) + v->FirstDataSector;
}

static int ReadCluster( FSDIRENTRY *d )
{
  int sector;

  sector = SectorFromCluster( d->driveid, d->CurrentCluster );
  if ( DISKIO_ReadSectors( d->driveid, d->clusterdata, sector, vfs[d->driveid]->SectorsPerCluster) != FS_SUCCESS )
    return FS_ERR_IO;

  d->CachedCluster = d->CurrentCluster;
  return FS_SUCCESS;
}

static int NextCluster( FSDIRENTRY *d )
{
  d->CurrentCluster = SWAP16(vfs[d->driveid]->FAT[d->CurrentCluster]);
  if ( d->CurrentCluster >= CLUSTER_END_CHAIN )
    return 0;

  return 1;
}

/****************************************************************************
* FindEntry
*
* Look through a directory tree looking for an active entry.
* The current cluster should be available in d->clusterdata
****************************************************************************/
static int FindEntry( FSDIRENTRY *d, int maxentries )
{
  int found = 0;
  unsigned char *direntry;
  VFATFS *v = vfs[d->driveid];
  SFNDIRREC *sfn;
  LFNDIRREC *lfn;
  static BYTE checksum = 0;

  if ( !d->crosscluster )
    {
      /* Clear names */
      memset(d->shortname, 0, 13);
      memset(d->longname, 0, MAX_LONG_NAME);
    }

  while( d->CurrentDirEntry < maxentries && !found )
    {
      /* Pointer to this directory entry */
      if ( d->CurrentCluster == ROOTCLUSTER )
        direntry = (v->rootDir + ( d->CurrentDirEntry << 5 ) );
      else
        direntry = (d->clusterdata + ( d->CurrentDirEntry << 5 ) );

      switch( direntry[0] )
        {
        case 0x00:
        case 0xE5:
          break;	/* Inactive entries */

        default:

          sfn = (SFNDIRREC *)direntry;
          d->crosscluster = 1;

          if ( ( sfn->attribute & ATTR_LONG_NAME_MASK ) == ATTR_LONG_NAME )
            {
              if ( direntry[0] & LFN_LAST_ENTRY )
                memset(&d->longname, 0, MAX_LONG_NAME);

              lfn = (LFNDIRREC *)direntry;
              BuildLongNameFromDirEntry( lfn, direntry[0] & LFN_ENTRY_MASK, d->longname);
              checksum = lfn->checksum;
            }
          else
            {
              /* Short name entry */
              found = 1;
              memcpy(&d->dirent, direntry, 32);
              BuildShortNameFromDirEntry( sfn, d->shortname );
              d->fsize = SWAP32(sfn->filesize);
              d->crosscluster = 0;
              /* Ensure long name is populated with something */
              if ( strlen((char *)d->longname) == 0 )
                {
                  strcpy((char *)d->longname, (char *)d->shortname);
                  return found;
                }
              else
                {
                  /* If checksums don't match - the FS is inconsistent
                                   To do no harm, skip this entry */
                  if ( checksum == CalcShortNameCheckSum(sfn->dirname) )
                    return found;
                  else
                    found = 0;
                }
            }
        }

      d->CurrentDirEntry++;

    }

  return found;
}

/****************************************************************************
* FindInRootDirectory
*
* Root directory is somewhat special. It's a fixed length and has no entry
* in the FAT as such.
*
* Logically, this should be the first 2 clusters, but the spec says it can
* be set to any size by the format utility (Think NT! FAT64/128/256)
*
* For speed, as all searches begin here, the root directory is held in 
* memory throughout.
*
* FSDIRENTRY should only have the drive id set.
****************************************************************************/
static int FindInRootDirectory( FSDIRENTRY *d, char *search )
{
  int found = 0;

  d->CurrentDirEntry++;

  while( (FindEntry(d, vfs[d->driveid]->RootDirEntries)) && !found )
    {
      if ( strcasecmp(search, (char *) d->shortname) == 0 )
        {
          found = 1;
        }

      if ( strcasecmp(search, (char *) d->longname) == 0 )
        {
          found = 1;
        }

      if ( !found )
        d->CurrentDirEntry++;
    }

  return found;
}

/****************************************************************************
* FindInClusters
*
* Generic routine to find a given name in a chain of clusters.
* Used for non-Root Directory searching
****************************************************************************/
static int FindInClusters( FSDIRENTRY *d, char *findme )
{
  int found = 0;

  if ( d->CurrentDirEntry == -1 )
    d->CurrentDirEntry = 0;

  /* While not at end of chain */
  while( !found && ( d->CurrentCluster < CLUSTER_END_CHAIN ) )
    {
      /* Retrieve dir entries looking for match */
      while( !found && (FindEntry( d, ( vfs[d->driveid]->BytesPerSector * vfs[d->driveid]->SectorsPerCluster) >> 5 ) ) )
        {
          if ( strcasecmp((char *)d->shortname, findme) == 0 )
            found = 1;
          if ( strcasecmp((char *)d->longname, findme) == 0 )
            found = 1;

          if (!found)
            d->CurrentDirEntry++;
        }

      /* Read next cluster */
      if ( !found )
        {
          if ( NextCluster(d) )
            {
              d->CurrentDirEntry = 0;
              ReadCluster(d);
            }
        }
    }

  return found;
}

/****************************************************************************
* VFAT_opendir
*
* Find the requested directory.
****************************************************************************/
int VFAT_opendir( int drive, FSDIRENTRY *d, char *search )
{
  char *p;
  char srchtmp[1024];
  int searchroot = 1;
  int found = 0;

  /* Clear out FSDIRENTRY */
  memset(d, 0, sizeof(FSDIRENTRY));

  /* Set drive and root */
  d->driveid = drive;
  d->CurrentCluster = ROOTCLUSTER;
  d->CurrentDirEntry = -1;

  /* Is this a request for root ? */
  if ( ( strlen(search) == 0 ) || ( strcmp(search,PSEPS) == 0 ) || ( strcmp(search, DIR_ROOT) == 0 ) )
    {
      return FS_FILE_OK;
    }

  /* Searching for a sub-directory */
  if ( search[0] == PSEP )
    strcpy(srchtmp, &search[1]);
  else
    strcpy(srchtmp, search);

  p = strtok(srchtmp, PSEPS);
  while ( p )
    {
      found = 0;
      if ( searchroot )
        {
          if ( !FindInRootDirectory(d, p) )
            return FS_NO_FILE;
          else
            {
              /* MUST be a directory */
              if ( d->dirent.attribute & ATTR_DIRECTORY )
                {
                  d->CurrentCluster = d->FirstCluster = SWAP16(d->dirent.fstClustLow);
                  d->CurrentDirEntry = -1;

                  /* Allocate the cluster for this data record */
                  d->clusterdata = (BYTE *)malloc(vfs[d->driveid]->SectorsPerCluster * vfs[d->driveid]->BytesPerSector);
                  ReadCluster(d);
                  found = 1;
                  searchroot = 0;
                }
              else
                return FS_NO_FILE;
            }
        }
      else
        {
          if ( FindInClusters( d, p ) )
            {
              /* MUST be a directory */
              if ( !( d->dirent.attribute & ATTR_DIRECTORY ) )
                {
                  free(d->clusterdata);
                  return FS_NO_FILE;
                }

              /* Read up this cluster */
              d->CurrentCluster = d->FirstCluster = SWAP16(d->dirent.fstClustLow);
              d->CurrentDirEntry = 0;
              ReadCluster(d);
              found = 1;
            }
        }

      p = strtok(NULL, PSEPS);
    }

  if ( !found )
    {
      if ( d->clusterdata != NULL )
        {
          free(d->clusterdata);
          d->clusterdata = NULL;
        }
      return FS_NO_FILE;
    }

  return FS_FILE_OK;

}

/****************************************************************************
* VFAT_readdir
****************************************************************************/
int VFAT_readdir( FSDIRENTRY *d )
{
  int ret;

  d->CurrentDirEntry++;
  /* Are we in root ? */
  if ( d->CurrentCluster == ROOTCLUSTER )
    {
      if( FindEntry( d, vfs[d->driveid]->RootDirEntries ) )
        return FS_FILE_OK;
    }
  else
    {
      while( d->CurrentCluster < CLUSTER_END_CHAIN )
        {
          ret = FindEntry( d, ( vfs[d->driveid]->BytesPerSector * vfs[d->driveid]->SectorsPerCluster) >> 5 );

          if ( ret )
            return FS_FILE_OK;

          if ( NextCluster(d) )
            {
              d->CurrentDirEntry = 0;
              ReadCluster(d);
            }
        }
    }
  return FS_NO_FILE;
}

/****************************************************************************
* VFAT_closedir
****************************************************************************/
void VFAT_closedir( FSDIRENTRY *d )
{
  if ( d->clusterdata != NULL )
    {
      free(d->clusterdata);
      d->clusterdata = NULL;
    }
}

/****************************************************************************
* VFAT_fopen
*
* v0.1 - VFAT_READ_ONLY Supported
****************************************************************************/
int VFAT_fopen( int drive, FSDIRENTRY *d, char *fname, int mode )
{
  char filename[1024];
  char path[1024];
  char temp[1024];
  char *p;

  if ( drive < 0 || drive > 1 )
    return FS_NO_FILE;

  if ( mode != FS_READ )
    return FS_NO_FILE;

  /* Clear */
  memset(d, 0, sizeof(FSDIRENTRY));
  d->driveid = drive;

  path[0] = temp[0] = filename[0] = 0;

  if ( fname[0] == PSEP )
    strcpy(temp, &fname[1]);
  else
    strcpy(temp, fname);

  /* Split into filename and path */
  p = strrchr(temp, PSEP);
  if ( p )
    {
      /* Have path and filename */
      *p = 0;
      strcpy(path, temp);
      p++;
      strcpy(filename, p);
    }
  else
    strcpy(filename, temp);

  /* Do search */
  if ( strlen(path) )
    {
      if ( VFAT_opendir(drive, d, path) != FS_FILE_OK )
        {
          VFAT_closedir(d);
          return FS_NO_FILE;
        }

      if ( !FindInClusters( d, filename ) )
        {
          VFAT_closedir(d);
          return FS_NO_FILE;
        }
    }
  else
    {
      /* Much simpler check on root directory */
      d->CurrentCluster = ROOTCLUSTER;
      d->CurrentDirEntry = -1;
      if ( !FindInRootDirectory( d, filename ) )
        {
          VFAT_closedir(d);
          return FS_NO_FILE;
        }
      d->clusterdata = (BYTE *)malloc(vfs[d->driveid]->SectorsPerCluster * vfs[d->driveid]->BytesPerSector);
    }

  /* Must be a file only */
  if ( d->dirent.attribute & ( ATTR_DIRECTORY | ATTR_VOLUME_ID ) )
    {
      VFAT_closedir(d);
      return FS_NO_FILE;
    }

  d->FirstCluster = d->CurrentCluster = SWAP16(d->dirent.fstClustLow);
  d->CachedCluster = -1;

  return FS_FILE_OK;

}

/****************************************************************************
* VFAT_fclose
****************************************************************************/
void VFAT_fclose( FSDIRENTRY *d )
{
  VFAT_closedir(d);
}

/****************************************************************************
* VFAT_fread
****************************************************************************/
int VFAT_fread( FSDIRENTRY *d, void *buffer, int length )
{
  int cluster;
  int tbytes;
  int umask;
  int i;
  int bytesdone = 0;
  int reallength;
  BYTE *p = (BYTE *)buffer;

  if ( length <= 0 )
    return 0;

  /* Determine which cluster in the chain we are in */
  tbytes = ( vfs[d->driveid]->SectorsPerCluster * vfs[d->driveid]->BytesPerSector );
  umask = tbytes - 1;
  cluster = ( d->fpos / tbytes );

  /* Rewind current cluster */
  d->CurrentCluster = d->FirstCluster;

  /* Bring this cluster into view */
  for ( i = 0; i < cluster; i++ )
    d->CurrentCluster = SWAP16(vfs[d->driveid]->FAT[d->CurrentCluster]);

  /* Read the cluster */
  if ( d->CachedCluster != d->CurrentCluster )
    ReadCluster(d);

  /* Get real read length */
  reallength = ( d->fpos + length ) < d->fsize ? length : d->fsize - d->fpos;

  if ( reallength <= 0 )
    return 0;

  /* Move data */
  while( reallength )
    {
      if ( !(d->fpos & umask) && ( reallength >= tbytes ) )
        {
          /* Move a full cluster */
          memcpy(p + bytesdone, d->clusterdata, tbytes);
          reallength -= tbytes;
          bytesdone += tbytes;
          d->fpos += tbytes;
        }
      else
        {
          p[bytesdone++] = d->clusterdata[d->fpos & umask];
          d->fpos++;
          reallength--;
        }

      if ( !( d->fpos & umask ) )
        {
          if ( NextCluster(d) )
            {
              ReadCluster(d);
            }
          else
            return bytesdone;
        }
    }

  return bytesdone;
}

/****************************************************************************
* VFAT_fseek
****************************************************************************/
int VFAT_fseek( FSDIRENTRY *d, int where, int whence )
{
  switch( whence )
    {
    case SEEK_SET:
      if ( ( where >= 0 ) && ( where <= d->fsize ) )
        {
          d->fpos = where;
          return FS_FILE_OK;
        }
      break;

    case SEEK_CUR:
      if ( ( ( d->fpos + where ) >= 0 ) && ( ( d->fpos + where ) <= d->fsize ) )
        {
          d->fpos += where;
          return FS_FILE_OK;
        }
      break;

    case SEEK_END:
      if ( ( where <= 0 ) && ( abs(where) <= d->fsize ) )
        {
          d->fpos = d->fsize + where;
          return FS_FILE_OK;
        }
      break;
    }

  return FS_NO_FILE;
}

/****************************************************************************
* VFAT_ftell
*
* Return the current position of a file
****************************************************************************/
int VFAT_ftell( FSDIRENTRY *d )
{
  return d->fpos;
}

