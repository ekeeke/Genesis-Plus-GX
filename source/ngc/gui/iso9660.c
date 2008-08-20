/****************************************************************************
 * DVD ISO9660/Joliet Parsing
 *
 * This is not intended as a complete guide to ISO9660.
 * Here I use the bare minimum!
 ***************************************************************************/
#include "shared.h"
#include "dvd.h"
#include "iso9660.h"

/** Minimal ISO Directory Definition **/
#define RECLEN 0			/* Record length */
#define EXTENT 6			/* Extent */
#define FILE_LENGTH 14		/* File length (BIG ENDIAN) */
#define FILE_FLAGS 25		/* File flags */
#define FILENAME_LENGTH 32	/* Filename length */
#define FILENAME 33			/* ASCIIZ filename */

/** Minimal Primary Volume Descriptor **/
#define PVDROOT 0x9c

static int IsJoliet = 0;
u64 rootdir = 0;
u64 basedir = 0;
int rootdirlength = 0;

/** Global file entry table **/
FILEENTRIES filelist[MAXFILES];
static char dvdbuffer[2048] ATTRIBUTE_ALIGN (32);

/****************************************************************************
 * Primary Volume Descriptor
 *
 * The PVD should reside between sector 16 and 31.
 * This is for single session DVD only.
 ****************************************************************************/
int getpvd ()
{
	int sector = 16;
	u32 rootdir32;

	basedir = rootdirlength = 0;
	IsJoliet = -1;

	/** Look for Joliet PVD first **/
	while (sector < 32)
	{
		if (dvd_read (&dvdbuffer, 2048, (u64)(sector << 11)))
		{
			if (memcmp (&dvdbuffer, "\2CD001\1", 8) == 0)
			{
        memcpy(&rootdir32, &dvdbuffer[PVDROOT + EXTENT], 4);
				basedir = (u64)rootdir32;
				memcpy (&rootdirlength, &dvdbuffer[PVDROOT + FILE_LENGTH], 4);
				basedir <<= 11;
				IsJoliet = 1;
				break;
			}
		}
		else return 0; /*** Can't read sector! ***/
		sector++;
	}

  if (IsJoliet > 0) return 1; /*** Joliet PVD Found ? ***/

	/*** Look for standard ISO9660 PVD ***/
	sector = 16;
	while (sector < 32)
	{
		if (dvd_read (&dvdbuffer, 2048, (u64)(sector << 11)))
		{
			if (memcmp (&dvdbuffer, "\1CD001\1", 8) == 0)
			{
				memcpy (&rootdir32, &dvdbuffer[PVDROOT + EXTENT], 4);
				basedir = (u64)rootdir32;
				memcpy (&rootdirlength, &dvdbuffer[PVDROOT + FILE_LENGTH], 4);
				IsJoliet = 0;
				basedir <<= 11;
				break;
			}
		}
		else return 0; /*** Can't read sector! ***/
	    sector++;
	}

  return (IsJoliet == 0);
}

/****************************************************************************
 * getentry
 *
 * Support function to return the next file entry, if any
 * Declared static to avoid accidental external entry.
 ****************************************************************************/
static int diroffset = 0;
static int getentry (int entrycount)
{
	char fname[512]; /* Huge, but experience has determined this */
	char *ptr;
	char *filename;
	char *filenamelength;
	char *rr;
	int j;
	u32 offset32;

	/* Basic checks */
	if (entrycount >= MAXFILES) return 0;
	if (diroffset >= 2048) return 0;

	/** Decode this entry **/
	if (dvdbuffer[diroffset])	/* Record length available */
    {
		/* Update offsets into sector buffer */
		ptr = (char *) &dvdbuffer[0];
		ptr += diroffset;
		filename = ptr + FILENAME;
		filenamelength = ptr + FILENAME_LENGTH;

		/* Check for wrap round - illegal in ISO spec,
		 * but certain crap writers do it! */
		if ((diroffset + dvdbuffer[diroffset]) > 2048) return 0;

		if (*filenamelength)
		{
			memset (&fname, 0, 512);

			/*** Do ISO 9660 first ***/
			if (!IsJoliet) strcpy (fname, filename);
			else
			{
				/*** The more tortuous unicode joliet entries ***/
				for (j = 0; j < (*filenamelength >> 1); j++)
				{
					fname[j] = filename[j * 2 + 1];
				}

				fname[j] = 0;

				if (strlen (fname) >= MAXJOLIET) fname[MAXJOLIET - 1] = 0;
				if (strlen (fname) == 0) fname[0] = filename[0];
			}

			if (strlen (fname) == 0) strcpy (fname, ".");
			else
			{
				if (fname[0] == 1) strcpy (fname, "..");
				else
				{
					/*
					 * Move *filenamelength to t,
					 * Only to stop gcc warning for noobs :)
					 */
					int t = *filenamelength;
					fname[t] = 0;
				}
			}

			/** Rockridge Check **/
			rr = strstr (fname, ";");
			if (rr != NULL) *rr = 0;

			strcpy (filelist[entrycount].filename, fname);
			memcpy (&offset32, &dvdbuffer[diroffset + EXTENT], 4);
			filelist[entrycount].offset = (u64)offset32;
			memcpy (&filelist[entrycount].length, &dvdbuffer[diroffset + FILE_LENGTH], 4);
			memcpy (&filelist[entrycount].flags, &dvdbuffer[diroffset + FILE_FLAGS], 1);

			filelist[entrycount].offset <<= 11;
			filelist[entrycount].flags = filelist[entrycount].flags & 2;
			filelist[entrycount].filename_offset = 0;

			/*** Prepare for next entry ***/
			diroffset += dvdbuffer[diroffset];

			return 1;
		}
	}
	return 0;
}

/****************************************************************************
 * parseDVDdirectory
 *
 * This function will parse the directory tree.
 * It relies on rootdir and rootdirlength being pre-populated by a call to
 * getpvd, a previous parse or a menu selection.
 *
 * The return value is number of files collected, or 0 on failure.
 ****************************************************************************/
int parseDVDdirectory ()
{
	int pdlength;
	u64 pdoffset;
	u64 rdoffset;
	int len = 0;
	int filecount = 0;

	pdoffset = rdoffset = rootdir;
	pdlength = rootdirlength;
	filecount = 0;

	/** Clear any existing values ***/
	memset (&filelist, 0, sizeof (FILEENTRIES) * MAXFILES);

	/*** Get as many files as possible ***/
	while (len < pdlength)
	{
		if (dvd_read (&dvdbuffer, 2048, pdoffset) == 0) return 0;

		diroffset = 0;

		while (getentry (filecount))
		{
			if (filecount < MAXFILES) filecount++;
		}

		len += 2048;
		pdoffset = rdoffset + len;
	}
	return filecount;
}
