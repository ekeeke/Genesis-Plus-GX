/*
 *  file_mem.c
 *
 *  FAT and Memory Card SRAM/Savestate files managment
 *
 *  Softdev (2006)
 *  Eke-Eke (2007,2008,2009)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ***************************************************************************/

#include "shared.h"
#include "file_mem.h"
#include "file_fat.h"
#include "dvd.h"
#include "gui.h"
#include "filesel.h"
#include "saveicon.h"

/* Global ROM filename */
char rom_filename[MAXJOLIET];


/* Support for MemCards  */
/**
 * libOGC System Work Area
 */
static u8 SysArea[CARD_WORKAREA] ATTRIBUTE_ALIGN (32);
static card_dir CardDir;
static card_file CardFile;
static card_stat CardStatus;

/**
 * DMA Transfer Area.
 * Must be 32-byte aligned.
 * 64k SRAM + 2k Icon
 */
static u8 savebuffer[STATE_SIZE] ATTRIBUTE_ALIGN (32);


/****************************************************************************
 * SDCARD Access functions
 *
 * We use the same buffer as for Memory Card manager
 * Function returns TRUE on success.
 *****************************************************************************/
static int FAT_ManageFile(char *filename, u8 direction, u8 filetype)
{
  char fname[MAXPATHLEN];
  int done = 0;
  int filesize;

  /* build complete SDCARD filename */
  sprintf (fname, "%s/saves/%s", DEFAULT_PATH, filename);

  /* open file */
  FILE *fp = fopen(fname, direction ? "rb" : "wb");
  if (fp == NULL)
  {
    GUI_WaitPrompt("Error","Unable to open file !");
    return 0;
  }

  switch (direction)
  {
    case 0: /* SAVING */

      if (filetype) /* SRAM */
      {
        memcpy(savebuffer, sram.sram, 0x10000);
        sram.crc = crc32 (0, sram.sram, 0x10000);
        filesize = 0x10000;
      }
      else filesize = state_save(savebuffer); /* STATE */

      /* write buffer (2k blocks) */
      while (filesize > FATCHUNK)
      {
        fwrite(savebuffer + done, FATCHUNK, 1, fp);
        filesize -= FATCHUNK;
        done += FATCHUNK;
      }
      done += fwrite(savebuffer + done, filesize, 1, fp);
      fclose(fp);

      if (done < filesize)
      {
        GUI_WaitPrompt("Error","Unable to write file !");
        return 0;
      }

      sprintf (fname, "Saved %d bytes successfully", done);
      GUI_WaitPrompt("Information",fname);
      return 1;

    case 1: /* LOADING */

      /* read size */
      fseek(fp , 0 , SEEK_END);
      filesize = ftell (fp);
      fseek(fp, 0, SEEK_SET);

      /* read into buffer (2k blocks) */
      while (filesize > FATCHUNK)
      {
        fread(savebuffer + done, FATCHUNK, 1, fp);
        filesize -= FATCHUNK;
        done += FATCHUNK;
      }
      done += fread(savebuffer + done, filesize, 1, fp);
      fclose(fp);

      if (done < filesize)
      {
        GUI_WaitPrompt("Error","Unable to read file !");
        return 0;
      }

      if (filetype) /* SRAM */
      {
        memcpy(sram.sram, savebuffer, done);
        sram.crc = crc32 (0, sram.sram, 0x10000);
      }
      else
      {
        /* STATE */
        if (!state_load(savebuffer))
        {
          GUI_WaitPrompt("Error","File version is not compatible !");
          return 0;
        }
      }

      sprintf (fname, "Loaded %d bytes successfully", done);
      GUI_WaitPrompt("Information",fname);
      return 1;
  }

  return 0; 
}

/****************************************************************************
 * MountTheCard
 *
 * libOGC provides the CARD_Mount function, and it should be all you need.
 * However, experience with previous emulators has taught me that you are
 * better off doing a little bit more than that!
 *
 * Function returns TRUE on success.
 *****************************************************************************/
static int MountTheCard (u8 slot)
{
  int tries = 0;
  int CardError;
  *(unsigned long *) (0xcc006800) |= 1 << 13; /*** Disable Encryption ***/
#ifndef HW_RVL
  uselessinquiry ();
#endif
  while (tries < 10)
  {
    VIDEO_WaitVSync ();
    CardError = CARD_Mount (slot, SysArea, NULL); /*** Don't need or want a callback ***/
    if (CardError == 0) return 1;
    else EXI_ProbeReset ();
    tries++;
  }
  return 0;
}

/****************************************************************************
 * CardFileExists
 *
 * Wrapper to search through the files on the card.
 * Returns TRUE if found.
 ****************************************************************************/
static int CardFileExists (char *filename, u8 slot)
{
  int CardError = CARD_FindFirst (slot, &CardDir, TRUE);
  while (CardError != CARD_ERROR_NOFILE)
  {
    CardError = CARD_FindNext (&CardDir);
    if (strcmp ((char *) CardDir.filename, filename) == 0) return 1;
  }
  return 0;
}

/****************************************************************************
 * FILE autoload (SRAM/FreezeState or Config File)
 *
 *
 *****************************************************************************/
void memfile_autoload(s8 autosram, s8 autostate)
{
  /* this should be transparent to the user */
  SILENT = 1; 

  /* SRAM */
  if (autosram != -1)
    ManageSRAM(1,autosram);

  /* STATE */
  if (autostate != -1)
    ManageState(1,autostate);

  SILENT = 0;
}

void memfile_autosave(s8 autosram, s8 autostate)
{
  int crccheck = crc32 (0, sram.sram, 0x10000);

  /* this should be transparent to the user */
  SILENT = 1;
  
  /* SRAM */
  if ((autosram != -1) && (crccheck != sram.crc))
    ManageSRAM(0, autosram);

  /* STATE */
  if (autostate != -1)
    ManageState(0,autostate);

  SILENT = 0;
}


/****************************************************************************
 * ManageSRAM
 *
 * Here is the main SRAM Management stuff.
 * The output file contains an icon (2K), 64 bytes comment and the SRAM (64k).
 * As memcards are allocated in blocks of 8k or more, you have a around
 * 6k bytes to save/load any other data you wish without altering any of the
 * main save / load code.
 *
 * direction == 0 save, 1 load.
 ****************************************************************************/
int ManageSRAM (u8 direction, u8 device)
{
  if (!cart.romsize) return 0;

  char filename[MAXJOLIET];

  if (direction) GUI_MsgBoxOpen("Information","Loading SRAM ...",1);
  else GUI_MsgBoxOpen("Information","Saving SRAM ...",1);

  /* clean buffer */
  memset(savebuffer, 0, STATE_SIZE);

  if (device == 0)
  {
    /* FAT support */
    sprintf (filename, "%s.srm", rom_filename);
    return FAT_ManageFile(filename,direction,1);
  }

  /* Memory CARD support */
  char action[80];
  int CardError;
  unsigned int SectorSize;
  int blocks;
  char comment[2][32] = { {"Genesis Plus 1.2a"}, {"SRAM Save"} };
  int outbytes = 0;
  int sbo;
  unsigned long inzipped,outzipped;

  /* First, build a filename */
  sprintf (filename, "MD-%04X.srm", realchecksum);
  strcpy (comment[1], filename);

  /* set MCARD slot nr. */
  u8 CARDSLOT = device - 1;

  /* Saving */
  if (direction == 0)
  {
    /*** Build the output buffer ***/
    memcpy (&savebuffer, &icon, 2048);
    memcpy (&savebuffer[2048], &comment[0], 64);

    inzipped = 0x10000;
    outzipped = 0x12000;
    compress2 ((Bytef *) &savebuffer[2112+sizeof(outzipped)], &outzipped, (Bytef *) &sram.sram, inzipped, 9);
    memcpy(&savebuffer[2112], &outzipped, sizeof(outzipped));
  }

  outbytes = 2048 + 64 + outzipped + sizeof(outzipped);

  /*** Initialise the CARD system ***/
  memset (&SysArea, 0, CARD_WORKAREA);
  CARD_Init ("GENP", "00");

  /*** Attempt to mount the card ***/
  CardError = MountTheCard (CARDSLOT);

  if (CardError)
  {
    /*** Retrieve the sector size ***/
    CardError = CARD_GetSectorSize (CARDSLOT, &SectorSize);

    switch (direction)
    {
      case 0: /*** Saving ***/
        /*** Determine number of blocks on this card ***/
        blocks = (outbytes / SectorSize) * SectorSize;
        if (outbytes % SectorSize)  blocks += SectorSize;

        /*** Check if a previous save exists ***/
        if (CardFileExists (filename,CARDSLOT))
        {
          CardError = CARD_Open (CARDSLOT, filename, &CardFile);
          if (CardError)
          {
            sprintf (action, "Unable to open file (%d)", CardError);
            GUI_WaitPrompt("Error",action);
            CARD_Unmount (CARDSLOT);
            return 0;
          }

          int size = CardFile.len;
          CARD_Close (&CardFile);

          if (size < blocks)
          {
            /* new size is bigger: check if there is enough space left */
            CardError = CARD_Create (CARDSLOT, "TEMP", blocks-size, &CardFile);
            if (CardError)
            {
              sprintf (action, "Unable to create temporary file (%d)", CardError);
              GUI_WaitPrompt("Error",action);
              CARD_Unmount (CARDSLOT);
              return 0;
            }
            CARD_Close (&CardFile);
            CARD_Delete(CARDSLOT, "TEMP");
          }

          /* always delete existing slot */
          CARD_Delete(CARDSLOT, filename);
        }

        /*** Create a new slot ***/
        CardError = CARD_Create (CARDSLOT, filename, blocks, &CardFile);
        if (CardError)
        {
          sprintf (action, "Unable to create new file (%d)", CardError);
          GUI_WaitPrompt("Error",action);
          CARD_Unmount (CARDSLOT);
          return 0;
        }

        /*** Continue and save ***/
        CARD_GetStatus (CARDSLOT, CardFile.filenum, &CardStatus);
        CardStatus.icon_addr = 0x0;
        CardStatus.icon_fmt = 2;
        CardStatus.icon_speed = 1;
        CardStatus.comment_addr = 2048;
        CARD_SetStatus (CARDSLOT, CardFile.filenum, &CardStatus);

        /*** And write the blocks out ***/
        sbo = 0;
        while (outbytes > 0)
        {
          CardError = CARD_Write (&CardFile, &savebuffer[sbo], SectorSize, sbo);
          outbytes -= SectorSize;
          sbo += SectorSize;
        }

        CARD_Close (&CardFile);
        CARD_Unmount (CARDSLOT);
        sram.crc = crc32 (0, &sram.sram[0], 0x10000);
        sprintf (action, "Saved %d bytes successfully", blocks);
        GUI_WaitPrompt("Information",action);
        return 1;

      default: /*** Loading ***/
        if (!CardFileExists (filename,CARDSLOT))
        {
          GUI_WaitPrompt("Error","File does not exist !");
          CARD_Unmount (CARDSLOT);
          return 0;
        }

        memset (&CardFile, 0, sizeof (CardFile));
        CardError = CARD_Open (CARDSLOT, filename, &CardFile);
        if (CardError)
        {
          sprintf (action, "Unable to open file (%d)", CardError);
          GUI_WaitPrompt("Error",action);
          CARD_Unmount (CARDSLOT);
          return 0;
        }

        blocks = CardFile.len;
        if (blocks < SectorSize) blocks = SectorSize;
        if (blocks % SectorSize) blocks++;

        /*** Just read the file back in ***/
        sbo = 0;
        int size = blocks;
        while (blocks > 0)
        {
          CARD_Read (&CardFile, &savebuffer[sbo], SectorSize, sbo);
          sbo += SectorSize;
          blocks -= SectorSize;
        }
        CARD_Close (&CardFile);
        CARD_Unmount (CARDSLOT);

        /*** update SRAM ***/
        memcpy(&inzipped,&savebuffer[2112],sizeof(inzipped));
        outzipped = 0x10000;
        uncompress ((Bytef *) &sram.sram, &outzipped, (Bytef *) &savebuffer[2112+sizeof(inzipped)], inzipped);
        sram.crc = crc32 (0, &sram.sram[0], 0x10000);

        /*** Inform user ***/
        sprintf (action, "Loaded %d bytes successfully", size);
        GUI_WaitPrompt("Information",action);
        return 1;
    }
  }

  GUI_WaitPrompt("Error","Unable to mount memory card");
  return 0;
}

/****************************************************************************
 * ManageState
 *
 * Here is the main Freeze File Management stuff.
 * The output file contains an icon (2K), 64 bytes comment and the STATE (~128k)
 *
 * direction == 0 save, 1 load.
 ****************************************************************************/
int ManageState (u8 direction, u8 device)
{
  if (!cart.romsize) return 0;

  char filename[MAXJOLIET];

  if (direction) GUI_MsgBoxOpen("Information","Loading State ...",1);
  else GUI_MsgBoxOpen("Information","Saving State ...",1);

  /* clean buffer */
  memset(savebuffer, 0, STATE_SIZE);

  if (device == 0)
  {
    /* FAT support */
    sprintf (filename, "%s.gpz", rom_filename);
    return FAT_ManageFile(filename,direction,0);
  }

  /* Memory CARD support */
  char action[80];
  int CardError;
  unsigned int SectorSize;
  int blocks;
  char comment[2][32] = { {"Genesis Plus 1.2a [FRZ]"}, {"Freeze State"} };
  int outbytes = 0;
  int sbo;
  int state_size = 0;

  /* First, build a filename */
  sprintf (filename, "MD-%04X.gpz", realchecksum);
  strcpy (comment[1], filename);

  /* set MCARD slot nr. */
  u8 CARDSLOT = device - 1;

  /* Saving */
  if (direction == 0)
  {
    /* Build the output buffer */
    memcpy (&savebuffer, &icon, 2048);
    memcpy (&savebuffer[2048], &comment[0], 64);
    state_size = state_save(&savebuffer[2112]);
  }

  outbytes = 2048 + 64 + state_size;

  /*** Initialise the CARD system ***/
  memset (&SysArea, 0, CARD_WORKAREA);
  CARD_Init ("GENP", "00");

  /*** Attempt to mount the card ***/
  CardError = MountTheCard (CARDSLOT);

  if (CardError)
  {
    /*** Retrieve the sector size ***/
    CardError = CARD_GetSectorSize (CARDSLOT, &SectorSize);

    switch (direction)
    {
      case 0: /*** Saving ***/
        /*** Determine number of blocks on this card ***/
        blocks = (outbytes / SectorSize) * SectorSize;
        if (outbytes % SectorSize) blocks += SectorSize;

        /*** Check if a previous save exists ***/
        if (CardFileExists (filename, CARDSLOT))
        {
          CardError = CARD_Open (CARDSLOT, filename, &CardFile);
          if (CardError)
          {
            sprintf (action, "Unable to open file (%d)", CardError);
            GUI_WaitPrompt("Error",action);
            CARD_Unmount (CARDSLOT);
            return 0;
          }

          int size = CardFile.len;
          CARD_Close (&CardFile);

          if (size < blocks)
          {
            /* new size is bigger: check if there is enough space left */
            CardError = CARD_Create (CARDSLOT, "TEMP", blocks-size, &CardFile);
            if (CardError)
            {
              sprintf (action, "Unable to create temporary file (%d)", CardError);
              GUI_WaitPrompt("Error",action);
              CARD_Unmount (CARDSLOT);
              return 0;
            }
            CARD_Close (&CardFile);
            CARD_Delete(CARDSLOT, "TEMP");
          }

          /* always delete existing slot */
          CARD_Delete(CARDSLOT, filename);
        }

        /*** Create a new slot ***/
        CardError = CARD_Create (CARDSLOT, filename, blocks, &CardFile);
        if (CardError)
        {
          sprintf (action, "Unable to create new file (%d)", CardError);
          GUI_WaitPrompt("Error",action);
          CARD_Unmount (CARDSLOT);
          return 0;
        }
        
        /*** Continue and save ***/
        CARD_GetStatus (CARDSLOT, CardFile.filenum, &CardStatus);
        CardStatus.icon_addr = 0x0;
        CardStatus.icon_fmt = 2;
        CardStatus.icon_speed = 1;
        CardStatus.comment_addr = 2048;
        CARD_SetStatus (CARDSLOT, CardFile.filenum, &CardStatus);

        /*** And write the blocks out ***/
        sbo = 0;
        while (outbytes > 0)
        {
          CardError = CARD_Write (&CardFile, &savebuffer[sbo], SectorSize, sbo);
          outbytes -= SectorSize;
          sbo += SectorSize;
        }

        CARD_Close (&CardFile);
        CARD_Unmount (CARDSLOT);
        sprintf (action, "Saved %d bytes successfully", blocks);
        GUI_WaitPrompt("Information",action);
        return 1;

      default: /*** Loading ***/
        if (!CardFileExists (filename, CARDSLOT))
        {
          GUI_WaitPrompt("Error","File does not exist !");
          CARD_Unmount (CARDSLOT);
          return 0;
        }

        memset (&CardFile, 0, sizeof (CardFile));
        CardError = CARD_Open (CARDSLOT, filename, &CardFile);
        if (CardError)
        {
          sprintf (action, "Unable to open file (%d)", CardError);
          GUI_WaitPrompt("Error",action);
          CARD_Unmount (CARDSLOT);
          return 0;
        }

        blocks = CardFile.len;
        if (blocks < SectorSize) blocks = SectorSize;
        if (blocks % SectorSize) blocks++;

        /*** Just read the file back in ***/
        sbo = 0;
        int size = blocks;
        while (blocks > 0)
        {
          CARD_Read (&CardFile, &savebuffer[sbo], SectorSize, sbo);
          sbo += SectorSize;
          blocks -= SectorSize;
        }
        CARD_Close (&CardFile);
        CARD_Unmount (CARDSLOT);

        /*** Load State ***/
        state_load(&savebuffer[2112]);

        /*** Inform user ***/
        sprintf (action, "Loaded %d bytes successfully", size);
        GUI_WaitPrompt("Information",action);
        return 1;
    }
  }
  
  GUI_WaitPrompt("Error","Unable to mount memory card !");
  return 0;
}
