/*
 *  file_slot.c
 *
 *  FAT and Memory Card SRAM/State slots managment
 *
 *  Copyright Eke-Eke (2008-2011), based on original code from Softdev (2006)
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************/

#include "shared.h"
#include "file_slot.h"
#include "file_load.h"
#include "gui.h"
#include "filesel.h"
#include "saveicon.h"

/**
 * libOGC CARD System Work Area
 */
static u8 SysArea[CARD_WORKAREA] ATTRIBUTE_ALIGN (32);


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
#ifdef HW_RVL
  *(unsigned long *) (0xCD006800) |= 1 << 13; /*** Disable Encryption ***/
#else
  *(unsigned long *) (0xCC006800) |= 1 << 13; /*** Disable Encryption ***/
#endif
  while (tries < 10)
  {
    VIDEO_WaitVSync ();
    if (CARD_Mount(slot, SysArea, NULL) == CARD_ERROR_READY)
      return 1;
    else
      EXI_ProbeReset ();
    tries++;
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
  if (!strlen(rom_filename))
    return;
  
  SILENT = 1;
  slot_load(slot, device);
  SILENT = 0;
}

void slot_autosave(int slot, int device)
{
  if (!strlen(rom_filename))
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

  if (!device)
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
      sprintf(filename,"MD-%04X.gp%d", rominfo.realchecksum, slot - 1);
    else
      sprintf(filename,"MD-%04X.srm", rominfo.realchecksum);

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
      if (CARD_Open(device, filename, &CardFile) == CARD_ERROR_READY)
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

  if (!device)
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
    {
      sprintf (filename,"%s/saves/%s.srm", DEFAULT_PATH, rom_filename);
    }

    /* Delete file */
    ret = remove(filename);
  }
  else
  {
    /* Memory Card support */
    if (slot > 0)
      sprintf(filename,"MD-%04X.gp%d", rominfo.realchecksum, slot - 1);
    else
      sprintf(filename,"MD-%04X.srm", rominfo.realchecksum);

    /* Initialise the CARD system */
    memset(&SysArea, 0, CARD_WORKAREA);
    CARD_Init("GENP", "00");

    /* CARD slot */
    device--;

    /* Mount CARD */
    if (CardMount(device))
    {
      /* Delete file */
      ret = CARD_Delete(device,filename);
      CARD_Unmount(device);
    }
  }

  return ret;
}

int slot_load(int slot, int device)
{
  char filename[MAXPATHLEN];
  unsigned long filesize, done = 0;
  int offset = 0;
  u8 *savebuffer;

  if (slot > 0)
  {
    GUI_MsgBoxOpen("Information","Loading State ...",1);
  }
  else
  {
    if (!sram.on)
    {
      GUI_WaitPrompt("Error","SRAM is disabled !");
      return 0;
    }

    GUI_MsgBoxOpen("Information","Loading SRAM ...",1);
  }

  if (!device)
  {
    /* FAT support */
    if (slot > 0)
      sprintf (filename,"%s/saves/%s.gp%d", DEFAULT_PATH, rom_filename, slot - 1);
    else
      sprintf (filename,"%s/saves/%s.srm", DEFAULT_PATH, rom_filename);

    /* Open file */
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
      GUI_WaitPrompt("Error","Unable to open file !");
      return 0;
    }

    /* Read size */
    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* allocate buffer */
    savebuffer = (u8 *)memalign(32,filesize);
    if (!savebuffer)
    {
      GUI_WaitPrompt("Error","Unable to allocate memory !");
      fclose(fp);
      return 0;
    }

    /* Read into buffer (2k blocks) */
    while (filesize > CHUNKSIZE)
    {
      fread(savebuffer + done, CHUNKSIZE, 1, fp);
      done += CHUNKSIZE;
      filesize -= CHUNKSIZE;
    }

    /* Read remaining bytes */
    fread(savebuffer + done, filesize, 1, fp);
    done += filesize;
    fclose(fp);
  }  
  else
  {
    /* Memory Card support */
    if (slot > 0)
      sprintf(filename, "MD-%04X.gp%d", rominfo.realchecksum, slot - 1);
    else
      sprintf(filename, "MD-%04X.srm", rominfo.realchecksum);

    /* Initialise the CARD system */
    char action[64];
    memset(&SysArea, 0, CARD_WORKAREA);
    CARD_Init("GENP", "00");

    /* CARD slot */
    device--;

    /* Attempt to mount the card */
    if (!CardMount(device))
    {
      GUI_WaitPrompt("Error","Unable to mount memory card");
      return 0;
    }
    
    /* Retrieve the sector size */
    u32 SectorSize = 0;
    int CardError = CARD_GetSectorSize(device, &SectorSize);
    if (!SectorSize)
    {
      sprintf(action, "Invalid sector size (%d)", CardError);
      GUI_WaitPrompt("Error",action);
      CARD_Unmount(device);
      return 0;
    }

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

    /* Retrieve file size */
    filesize = CardFile.len;
    if (filesize % SectorSize)
    {
      filesize = ((filesize / SectorSize) + 1) * SectorSize;
    }

    /* Allocate buffer */
    savebuffer = (u8 *)memalign(32,filesize);
    if (!savebuffer)
    {
      GUI_WaitPrompt("Error","Unable to allocate memory !");
      CARD_Close(&CardFile);
      CARD_Unmount(device);
      return 0;
    }

    /* Read file sectors */
    while (filesize > 0)
    {
      CARD_Read(&CardFile, &savebuffer[done], SectorSize, done);
      done += SectorSize;
      filesize -= SectorSize;
    }

    CARD_Close(&CardFile);
    CARD_Unmount(device);
    offset = 2112;
  }

  if (slot > 0)
  {
    /* Uncompress state buffer */
    u8 *state = (u8 *)memalign(32, STATE_SIZE);
    if (!state)
    {
      free(savebuffer);
      GUI_WaitPrompt("Error","Unable to allocate memory !");
      return 0;
    }
    done = STATE_SIZE;
    memcpy(&filesize, savebuffer + offset, 4);
    uncompress ((Bytef *)state, &done, (Bytef *)(savebuffer + offset + 4), filesize);

    /* Load state */
    if (state_load(state) <= 0)
    {
      free(state);
      free(savebuffer);
      GUI_WaitPrompt("Error","Invalid state file !");
      return 0;
    }

    free(state);
  }
  else
  {
    /* Load SRAM & update CRC */
    memcpy(sram.sram, &savebuffer[offset], 0x10000);
    sram.crc = crc32(0, sram.sram, 0x10000);
  }

  free(savebuffer);
  GUI_MsgBoxClose();
  return 1;
}


int slot_save(int slot, int device)
{
  char filename[MAXPATHLEN];
  unsigned long filesize, done = 0;
  int offset = device ? 2112 : 0;
  u8 *savebuffer;

  if (slot > 0)
  {
    /* allocate buffers */
    savebuffer = (u8 *)memalign(32,STATE_SIZE);
    u8 *state = (u8 *)memalign(32,STATE_SIZE);
    if (!savebuffer || !state)
    {
      GUI_WaitPrompt("Error","Unable to allocate memory !");
      return 0;
    }

    /* save state */
    GUI_MsgBoxOpen("Information","Saving State ...",1);
    done = state_save(state);

    /* compress state file */
    filesize = STATE_SIZE;
    compress2 ((Bytef *)(savebuffer + offset + 4), &filesize, (Bytef *)state, done, 9);
    memcpy(savebuffer + offset, &filesize, 4);
    filesize += 4;
    done = 0;
    free(state);
  }
  else
  {
    if (!sram.on)
    {
       GUI_WaitPrompt("Error","SRAM is disabled !");
       return 0;
    }

    /* allocate buffer */
    savebuffer = (u8 *)memalign(32,0x10000+offset);
    if (!savebuffer)
    {
      GUI_WaitPrompt("Error","Unable to allocate memory !");
      return 0;
    }

    GUI_MsgBoxOpen("Information","Saving SRAM ...",1);
    memcpy(&savebuffer[offset], sram.sram, 0x10000);
    sram.crc = crc32(0, sram.sram, 0x10000);
    filesize = 0x10000;
  }

  if (!device)
  {
    /* FAT support */
    if (slot > 0)
      sprintf(filename, "%s/saves/%s.gp%d", DEFAULT_PATH, rom_filename, slot - 1);
    else
      sprintf(filename, "%s/saves/%s.srm", DEFAULT_PATH, rom_filename);

    /* Open file */
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
      GUI_WaitPrompt("Error","Unable to open file !");
      free(savebuffer);
      return 0;
    }

    /* Write from buffer (2k blocks) */
    while (filesize > CHUNKSIZE)
    {
      fwrite(savebuffer + done, CHUNKSIZE, 1, fp);
      done += CHUNKSIZE;
      filesize -= CHUNKSIZE;
    }

    /* Write remaining bytes */
    fwrite(savebuffer + done, filesize, 1, fp);
    done += filesize;
    fclose(fp);
  }
  else
  {
    /* Memory Card support */
    if (slot > 0)
      sprintf(filename, "MD-%04X.gp%d", rominfo.realchecksum, slot - 1);
    else
      sprintf(filename, "MD-%04X.srm", rominfo.realchecksum);

    /* Initialise the CARD system */
    char action[64];
    memset(&SysArea, 0, CARD_WORKAREA);
    CARD_Init("GENP", "00");

    /* CARD slot */
    device--;

    /* Attempt to mount the card */
    if (!CardMount(device))
    {
      GUI_WaitPrompt("Error","Unable to mount memory card");
      free(savebuffer);
      return 0;
    }

    /* Retrieve the sector size */
    u32 SectorSize = 0;
    int CardError = CARD_GetSectorSize(device, &SectorSize);
    if (!SectorSize)
    {
      sprintf(action, "Invalid sector size (%d)", CardError);
      GUI_WaitPrompt("Error",action);
      CARD_Unmount(device);
      free(savebuffer);
      return 0;
    }

    /* Build the output buffer */
    char comment[2][32] = { {"Genesis Plus GX"}, {"SRAM Save"} };
    strcpy (comment[1], filename);
    memcpy (&savebuffer[0], &icon, 2048);
    memcpy (&savebuffer[2048], &comment[0], 64);

    /* Adjust file size */
    filesize += 2112;
    if (filesize % SectorSize)
    {
      filesize = ((filesize / SectorSize) + 1) * SectorSize;
    }

    /* Check if file already exists */
    card_file CardFile;
    if (CARD_Open(device, filename, &CardFile) == CARD_ERROR_READY)
    {
      int size = filesize - CardFile.len;
      CARD_Close(&CardFile);
      memset(&CardFile,0,sizeof(CardFile));

      /* Check file new size */
      if (size > 0)
      {
        CardError = CARD_Create(device, "TEMP", size, &CardFile);
        if (CardError)
        {
          sprintf(action, "Unable to increase file size (%d)", CardError);
          GUI_WaitPrompt("Error",action);
          CARD_Unmount(device);
          free(savebuffer);
          return 0;
        }

        /* delete temporary file */
        CARD_Close(&CardFile);
        memset(&CardFile,0,sizeof(CardFile));
        CARD_Delete(device, "TEMP");
      }

      /* delete previously existing file */
      CARD_Delete(device, filename);
    }

    /* Create a new file */
    CardError = CARD_Create(device, filename, filesize, &CardFile);
    if (CardError)
    {
      sprintf(action, "Unable to create file (%d)", CardError);
      GUI_WaitPrompt("Error",action);
      CARD_Unmount(device);
      free(savebuffer);
      return 0;
    }

    /* Update file informations */
    time_t rawtime;
    time(&rawtime);
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

  GUI_MsgBoxClose();
  free(savebuffer);

  /* Save screenshot */
  if (slot && !device)
  {
    sprintf(filename,"%s/saves/%s__%d.png", DEFAULT_PATH, rom_filename, slot - 1);
    gxSaveScreenshot(filename);
  }

  return 1;
}
