/*
 *  file_slot.c
 *
 *  FAT and Memory Card SRAM/State slots managment
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
#include "file_slot.h"
#include "file_fat.h"
#include "dvd.h"
#include "gui.h"
#include "filesel.h"
#include "saveicon.h"

/**
 * libOGC CARD System Work Area
 */
static u8 SysArea[CARD_WORKAREA] ATTRIBUTE_ALIGN (32);

/**
 * DMA Transfer Area.
 * Must be 32-byte aligned.
 * 64k SRAM + 2k Icon
 */
static u8 savebuffer[STATE_SIZE] ATTRIBUTE_ALIGN (32);


/****************************************************************************
 * CardMount
 *
 * libOGC provides the CARD_Mount function, and it should be all you need.
 * However, experience with previous emulators has taught me that you are
 * better off doing a little bit more than that!
 *
 *****************************************************************************/
static int CardMount(int slot)
{
  int tries = 0;
  int CardError;
#if defined(HW_DOL)
  *(unsigned long *) (0xCC006800) |= 1 << 13; /*** Disable Encryption ***/
  uselessinquiry ();
#elif defined(HW_RVL)
  *(unsigned long *) (0xCD006800) |= 1 << 13; /*** Disable Encryption ***/
#endif
  while (tries < 10)
  {
    VIDEO_WaitVSync ();
    CardError = CARD_Mount (slot, SysArea, NULL); /*** Don't need or want a callback ***/
    if (CardError == 0)
      return 1;
    else
      EXI_ProbeReset ();
    tries++;
  }
  return 0;
}

/****************************************************************************
 * CardFileExists
 *
 * Wrapper to search through the files on the card.
 ****************************************************************************/
static int CardFileExists (char *filename, int slot)
{
  card_dir CardDir;
  int CardError = CARD_FindFirst (slot, &CardDir, TRUE);
  while (CardError >= 0)
  {
    CardError = CARD_FindNext (&CardDir);
    if (strcmp ((char *) CardDir.filename, filename) == 0)
      return 1;
  }
  return 0;
}

/****************************************************************************
 * Slot Management
 *
 *
 ****************************************************************************/

void slot_autoload(int slot, int device)
{
  if (!cart.romsize)
    return;
  
  SILENT = 1;
  slot_load(slot, device);
  SILENT = 0;
}

void slot_autosave(int slot, int device)
{
  if (!cart.romsize)
    return;

  /* only save if SRAM changed */
  if (!slot && (crc32(0, &sram.sram[0], 0x10000) == sram.crc))
      return;

  SILENT = 1;
  slot_save(slot, device);
  SILENT = 0;
}

void slot_autodetect(int slot, int device, t_slot *ptr)
{
  if (!ptr)
    return;
  
  char filename[MAXPATHLEN];
  memset(ptr,0,sizeof(t_slot));

  if (device == 0)
  {
    /* FAT support */
    if (slot > 0)
      sprintf (filename,"%s/saves/%s.gp%d", DEFAULT_PATH, rom_filename, slot - 1);
    else
      sprintf (filename,"%s/saves/%s.srm", DEFAULT_PATH, rom_filename);

    /* Open file */
    FILE *fp = fopen(filename, "rb");
    if (fp)
    {
      /* Retrieve date & close */
	    struct stat filestat;
			stat(filename, &filestat);
      struct tm *timeinfo = localtime(&filestat.st_mtime);
      ptr->year = 1900 + timeinfo->tm_year;
      ptr->month = timeinfo->tm_mon;
      ptr->day = timeinfo->tm_mday;
      ptr->hour = timeinfo->tm_hour;
      ptr->min = timeinfo->tm_min;
      fclose(fp);
      ptr->valid = 1;
    }
  }
  else
  {
    /* Memory Card support */
    if (slot > 0)
      sprintf(filename,"MD-%04X.gp%d", realchecksum, slot - 1);
    else
      sprintf(filename,"MD-%04X.srm", realchecksum);

    /* Initialise the CARD system */
    memset(&SysArea, 0, CARD_WORKAREA);
    CARD_Init("GENP", "00");

    /* CARD slot */
    device--;

    /* Mount CARD */
    if (CardMount(device))
    {
      /* Open file */
      card_file CardFile;
      if (CARD_Open(device, filename, &CardFile))
      {
        /* Retrieve date & close */
        card_stat CardStatus;
        CARD_GetStatus(device, CardFile.filenum, &CardStatus);
        time_t rawtime = CardStatus.time;
	      struct tm *timeinfo = localtime(&rawtime);
        ptr->year = 1900 + timeinfo->tm_year;
        ptr->month = timeinfo->tm_mon;
        ptr->day = timeinfo->tm_mday;
        ptr->hour = timeinfo->tm_hour;
        ptr->min = timeinfo->tm_min;
        CARD_Close(&CardFile);
        ptr->valid = 1;
      }
      CARD_Unmount(device);
    }
  }
}

int slot_delete(int slot, int device)
{
  char filename[MAXPATHLEN];
  int ret = 0;

  if (device == 0)
  {
    /* FAT support */
    if (slot > 0)
    {
      /* remove screenshot */
      sprintf(filename,"%s/saves/%s__%d.png", DEFAULT_PATH, rom_filename, slot - 1);
      remove(filename);

      sprintf (filename,"%s/saves/%s.gp%d", DEFAULT_PATH, rom_filename, slot - 1);
    }
    else
      sprintf (filename,"%s/saves/%s.srm", DEFAULT_PATH, rom_filename);

    /* Delete file */
    ret = remove(filename);
  }
  else
  {
    /* Memory Card support */
    if (slot > 0)
      sprintf(filename,"MD-%04X.gp%d", realchecksum, slot - 1);
    else
      sprintf(filename,"MD-%04X.srm", realchecksum);

    /* Initialise the CARD system */
    memset(&SysArea, 0, CARD_WORKAREA);
    CARD_Init("GENP", "00");

    /* CARD slot */
    device--;

    /* Mount CARD */
    if (CardMount(device))
    {
      /* Open file */
      if (CardFileExists(filename, device))
        ret = CARD_Delete(device,filename);
      CARD_Unmount(device);
    }
  }

  return ret;
}

int slot_load(int slot, int device)
{
  char filename[MAXPATHLEN];
  int filesize, done = 0;
  int offset = device ? 2112 : 0;

  if (slot > 0)
    GUI_MsgBoxOpen("Information","Loading State ...",1);
  else
    GUI_MsgBoxOpen("Information","Loading SRAM ...",1);

  /* clean buffer */
  memset(savebuffer, 0, STATE_SIZE);

  if (device == 0)
  {
    /* FAT support */
    if (slot > 0)
      sprintf (filename,"%s/saves/%s.gp%d", DEFAULT_PATH, rom_filename, slot - 1);
    else
      sprintf (filename,"%s/saves/%s.srm", DEFAULT_PATH, rom_filename);

    /* Open file */
    FILE *fp = fopen(filename, "rb");
    if (fp)
    {
      /* Read size */
      fseek(fp , 0 , SEEK_END);
      filesize = ftell (fp);
      fseek(fp, 0, SEEK_SET);

      /* Read into buffer (2k blocks) */
      while (filesize > FATCHUNK)
      {
        fread(savebuffer + done, FATCHUNK, 1, fp);
        done += FATCHUNK;
        filesize -= FATCHUNK;
      }

      /* Read remaining bytes */
      fread(savebuffer + done, filesize, 1, fp);
      done += filesize;
      fclose(fp);
    }
    else
    {
      GUI_WaitPrompt("Error","Unable to open file !");
      return 0;
    }
  }  
  else
  {
    /* Memory Card support */
    if (index > 0)
      sprintf(filename, "MD-%04X.gp%d", realchecksum, slot - 1);
    else
      sprintf(filename, "MD-%04X.srm", realchecksum);

    /* Initialise the CARD system */
    char action[80];
    memset(&SysArea, 0, CARD_WORKAREA);
    CARD_Init("GENP", "00");

    /* CARD slot */
    device--;

    /* Attempt to mount the card */
    if (CardMount(device))
    {
      /* Retrieve the sector size */
      u32 SectorSize = 0;
      int CardError = CARD_GetSectorSize(device, &SectorSize);
      if (SectorSize)
      {
        /* Open file */
        card_file CardFile;
        CardError = CARD_Open(device, filename, &CardFile);
        if (CardError)
        {
          sprintf(action, "Unable to open file (%d)", CardError);
          GUI_WaitPrompt("Error",action);
          CARD_Unmount(device);
          return 0;
        }

        /* Get file size */
        filesize = CardFile.len;
        if (filesize % SectorSize)
          filesize = ((filesize / SectorSize) + 1) * SectorSize;

        /* Read file sectors */
        while (filesize > 0)
        {
          CARD_Read(&CardFile, &savebuffer[done], SectorSize, done);
          done += SectorSize;
          filesize -= SectorSize;
        }

        CARD_Close(&CardFile);
        CARD_Unmount(device);
      }
      else
      {
        sprintf(action, "Invalid sector size (%d)", CardError);
        GUI_WaitPrompt("Error",action);
        CARD_Unmount(device);
        return 0;
      }
    }
    else
    {
      GUI_WaitPrompt("Error","Unable to mount memory card");
      return 0;
    }
  }

  if (slot > 0)
  {
    /* Load state */
    if (!state_load(&savebuffer[offset]))
    {
      GUI_WaitPrompt("Error","Version is not compatible !");
      return 0;
    }
  }
  else
  {
    /* Load SRAM & update CRC */
    memcpy(sram.sram, &savebuffer[offset], 0x10000);
    sram.crc = crc32(0, &sram.sram[0], 0x10000);
  }

  GUI_MsgBoxClose();
  return 1;
}


int slot_save(int slot, int device)
{
  char filename[MAXPATHLEN];
  int filesize, done = 0;
  int offset = device ? 2112 : 0;

  /* clean buffer */
  memset(savebuffer, 0, STATE_SIZE);

  if (slot > 0)
  {
    GUI_MsgBoxOpen("Information","Saving State ...",1);
    filesize = state_save(&savebuffer[offset]);
  }
  else
  {
    GUI_MsgBoxOpen("Information","Saving SRAM ...",1);
    memcpy(&savebuffer[offset], sram.sram, 0x10000);
    sram.crc = crc32(0, &sram.sram[0], 0x10000);
    filesize = 0x10000;
  }

  if (device == 0)
  {
    /* FAT support */
    if (slot > 0)
      sprintf(filename, "%s/saves/%s.gp%d", DEFAULT_PATH, rom_filename, slot - 1);
    else
      sprintf(filename, "%s/saves/%s.srm", DEFAULT_PATH, rom_filename);

    /* Open file */
    FILE *fp = fopen(filename, "wb");
    if (fp)
    {
      /* Read into buffer (2k blocks) */
      while (filesize > FATCHUNK)
      {
        fwrite(savebuffer + done, FATCHUNK, 1, fp);
        done += FATCHUNK;
        filesize -= FATCHUNK;
      }

      /* Write remaining bytes */
      fwrite(savebuffer + done, filesize, 1, fp);
      done += filesize;
      fclose(fp);

      if (slot)
      {
        /* save screenshot */
        sprintf(filename,"%s/saves/%s__%d.png", DEFAULT_PATH, rom_filename, slot - 1);
        gxSaveScreenshot(filename);
      }
    }
    else
    {
      GUI_WaitPrompt("Error","Unable to open file !");
      return 0;
    }
  }  
  else
  {
    /* Memory Card support */
    if (index > 0)
      sprintf(filename, "MD-%04X.gp%d", realchecksum, slot - 1);
    else
      sprintf(filename, "MD-%04X.srm", realchecksum);

    /* Initialise the CARD system */
    char action[80];
    memset(&SysArea, 0, CARD_WORKAREA);
    CARD_Init("GENP", "00");

    /* CARD slot */
    device--;

    /* Attempt to mount the card */
    if (CardMount(device))
    {
      /* Retrieve the sector size */
      u32 SectorSize = 0;
      int CardError = CARD_GetSectorSize(device, &SectorSize);
      if (SectorSize)
      {
        /* Build the output buffer */
        char comment[2][32] = { {"Genesis Plus GX"}, {"SRAM Save"} };
        strcpy (comment[1], filename);
        memcpy (&savebuffer[0], &icon, 2048);
        memcpy (&savebuffer[2048], &comment[0], 64);

        /* Adjust file size */
        filesize += 2112;
        if (filesize % SectorSize)
          filesize = ((filesize / SectorSize) + 1) * SectorSize;

        /* Check if file already exists */
        card_file CardFile;
        if (CardFileExists(filename,device))
        {
          CardError = CARD_Open(device, filename, &CardFile);
          if (CardError)
          {
            sprintf(action, "Unable to open file (%d)", CardError);
            GUI_WaitPrompt("Error",action);
            CARD_Unmount(device);
            return 0;
          }
          
          int size = filesize - CardFile.len;
          CARD_Close(&CardFile);
          memset(&CardFile,0,sizeof(CardFile));
          if (size > 0)
          {
            /* new file is bigger: check if there is enough space left */
            CardError = CARD_Create(device, "TEMP", size, &CardFile);
            if (CardError)
            {
              sprintf(action, "Unable to create temporary file (%d)", CardError);
              GUI_WaitPrompt("Error",action);
              CARD_Unmount(device);
              return 0;
            }
            CARD_Close(&CardFile);
            CARD_Delete(device, "TEMP");
            memset(&CardFile,0,sizeof(CardFile));
          }

          /* delete previously existing slot */
          CARD_Delete(device, filename);
        }

        /* Create a new slot */
        CardError = CARD_Create(device, filename, filesize, &CardFile);
        if (CardError)
        {
          sprintf(action, "Unable to create file (%d)", CardError);
          GUI_WaitPrompt("Error",action);
          CARD_Unmount(device);
          return 0;
        }

        /* Get current time */
        time_t rawtime;
        time(&rawtime);
        
        /* Update file informations */
        card_stat CardStatus;
        CARD_GetStatus(device, CardFile.filenum, &CardStatus);
        CardStatus.icon_addr = 0x0;
        CardStatus.icon_fmt = 2;
        CardStatus.icon_speed = 1;
        CardStatus.comment_addr = 2048;
        CardStatus.time = rawtime;
        CARD_SetStatus(device, CardFile.filenum, &CardStatus);

        /* Write file sectors */
        while (filesize > 0)
        {
          CARD_Write(&CardFile, &savebuffer[done], SectorSize, done);
          filesize -= SectorSize;
          done += SectorSize;
        }

        /* Close file */
        CARD_Close(&CardFile);
        CARD_Unmount(device);
      }
      else
      {
        sprintf(action, "Invalid sector size (%d)", CardError);
        GUI_WaitPrompt("Error",action);
        CARD_Unmount(device);
        return 0;
      }
    }
    else
    {
      GUI_WaitPrompt("Error","Unable to mount memory card");
      return 0;
    }
  }

  GUI_MsgBoxClose();
  return 1;
}
