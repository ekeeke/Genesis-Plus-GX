/****************************************************************************
 * ROM Selection Interface
 *
 * The following features are implemented:
 *   . SDCARD access with LFN support (through softdev's VFAT library)
 *   . DVD access
 *   . easy subdirectory browsing
 *   . ROM browser
 *   . alphabetical file sorting (Marty Disibio)
 *   . load from history list (Marty Disibio)
 *
 ***************************************************************************/
#include "shared.h"
#include "dvd.h"
#include "iso9660.h"
#include "font.h"
#include "unzip.h"
#include "history.h"

#define PAGESIZE 12

static int maxfiles;
static int offset = 0;
static int selection = 0;
static int old_selection = 0;
static int old_offset = 0;
static char rootSDdir[256];
static u8 haveDVDdir = 0;
static u8 haveSDdir  = 0;
static u8 UseSDCARD = 0;
static u8 UseHistory = 0;
static int LoadFile (unsigned char *buffer);

/* globals */
FILE *sdfile;


/***************************************************************************
 * FileSortCallback
 *
 * Quick sort callback to sort file entries with the following order:
 *   .
 *   ..
 *   <dirs>
 *   <files>
 ***************************************************************************/ 
static int FileSortCallback(const void *f1, const void *f2)
{
	/* Special case for implicit directories */
	if(((FILEENTRIES *)f1)->filename[0] == '.' || ((FILEENTRIES *)f2)->filename[0] == '.')
	{
		if(strcmp(((FILEENTRIES *)f1)->filename, ".") == 0) { return -1; }
		if(strcmp(((FILEENTRIES *)f2)->filename, ".") == 0) { return 1; }
		if(strcmp(((FILEENTRIES *)f1)->filename, "..") == 0) { return -1; }
		if(strcmp(((FILEENTRIES *)f2)->filename, "..") == 0) { return 1; }
	}
	
	/* If one is a file and one is a directory the directory is first. */
	if(((FILEENTRIES *)f1)->flags == 1 && ((FILEENTRIES *)f2)->flags == 0) return -1;
	if(((FILEENTRIES *)f1)->flags == 0 && ((FILEENTRIES *)f2)->flags == 1) return 1;
	
	return stricmp(((FILEENTRIES *)f1)->filename, ((FILEENTRIES *)f2)->filename);
}

/***************************************************************************
 * ShowFiles
 *
 * Show filenames list in current directory
 ***************************************************************************/ 
static void ShowFiles (int offset, int selection) 
{
  int i, j;
  char text[MAXJOLIET+2];

  ClearScreen ();
  j = 0;

  for (i = offset; i < (offset + PAGESIZE) && (i < maxfiles); i++)
  {
    memset(text,0,MAXJOLIET+2);
    if (filelist[i].flags) sprintf(text, "[%s]", filelist[i].filename + filelist[i].filename_offset);
    else sprintf (text, "%s", filelist[i].filename + filelist[i].filename_offset);
    if (j == (selection - offset)) WriteCentre_HL ((j * fheight) + 120, text);
    else WriteCentre ((j * fheight) + 120, text);
    j++;
  }
  SetScreen ();
}

/***************************************************************************
 * updateSDdirname
 *
 * Update ROOT directory while browsing SDCARD
 ***************************************************************************/ 
static int updateSDdirname()
{
  int size=0;
  char *test;
  char temp[1024];
  
  /* current directory doesn't change */
  if (strcmp(filelist[selection].filename,".") == 0) return 0;
  
  /* go up to parent directory */
  else if (strcmp(filelist[selection].filename,"..") == 0)
  {
    /* determine last subdirectory namelength */
    sprintf(temp,"%s",rootSDdir);
    test= strtok(temp,"/");
    while (test != NULL)
    {
      size = strlen(test);
      test = strtok(NULL,"/");
    }

    /* remove last subdirectory name */
    size = strlen(rootSDdir) - size;
    rootSDdir[size-1] = 0;
  }
  else
  {
    sprintf(rootSDdir, "%s%s/",rootSDdir, filelist[selection].filename);
  }

  return 1;
}

/***************************************************************************
 * parseSDdirectory
 *
 * List files into one SDCARD directory
 ***************************************************************************/ 
static int parseSDdirectory()
{
  int nbfiles = 0;
  char filename[MAXPATHLEN];
  struct stat filestat;

  /* open directory */
  DIR_ITER *dir = diropen (rootSDdir);
  if (dir == NULL) 
  {
    sprintf(filename, "Error opening %s", rootSDdir);
    WaitPrompt (filename);
    return 0;
  }

  while (dirnext(dir, filename, &filestat) == 0)
  {
    if (strcmp(filename,".") != 0)
    {
      memset(&filelist[nbfiles], 0, sizeof (FILEENTRIES));
      sprintf(filelist[nbfiles].filename,"%s",filename);
      filelist[nbfiles].length = filestat.st_size;
      filelist[nbfiles].flags = (filestat.st_mode & S_IFDIR) ? 1 : 0;
      nbfiles++;
    }
  }

  dirclose(dir);

  /* Sort the file list */
  qsort(filelist, nbfiles, sizeof(FILEENTRIES), FileSortCallback);

  return nbfiles;
}

/****************************************************************************
 * FileSelected
 *
 * Called when a file is selected by the user inside the FileSelector loop.
 ****************************************************************************/ 
static void FileSelected()
{		
	/* If loading from history then we need to setup a few more things. */
	if(UseHistory)
	{	
		/* Get the parent folder for the file. */
		strncpy(rootSDdir, history.entries[selection].filepath, MAXJOLIET-1);
		rootSDdir[MAXJOLIET-1] = '\0';
	
		/* Get the length of the file. This has to be done
		 * before calling LoadFile().  */
		char filepath[MAXJOLIET];
		struct stat filestat;
		snprintf(filepath, MAXJOLIET-1, "%s%s", history.entries[selection].filepath, history.entries[selection].filename);
		filepath[MAXJOLIET-1] = '\0';			
		if(stat(filepath, &filestat) == 0)
		{
			filelist[selection].length = filestat.st_size;
		}	
	}

	/* Add/move the file to the top of the history. */
	history_add_file(rootSDdir, filelist[selection].filename);
	
	rootdir = filelist[selection].offset;
	rootdirlength = filelist[selection].length;
	memfile_autosave();
  genromsize = LoadFile(cart_rom);
	reloadrom();
  memfile_autoload();
}

/****************************************************************************
 * FileSelector
 *
 * Let user select a file from the File listing
 ****************************************************************************/ 
static void FileSelector () 
{
  short p;
  int haverom = 0;
  int redraw = 1;
  int go_up = 0;
  int i,size;
  
  while (haverom == 0)
  {
    if (redraw) ShowFiles (offset, selection);
    redraw = 0;
    p = ogc_input__getMenuButtons();
    
    /* scroll displayed filename */
    if (p & PAD_BUTTON_LEFT)
    {
      if (filelist[selection].filename_offset > 0)
      {
        filelist[selection].filename_offset --;
        redraw = 1;
      }
    }
    else if (p & PAD_BUTTON_RIGHT)
    {
      size = 0;
      for (i=filelist[selection].filename_offset; i<strlen(filelist[selection].filename); i++)
        size += font_size[(int)filelist[selection].filename[i]];
        
      if (size > back_framewidth)
      {
        filelist[selection].filename_offset ++;
        redraw = 1;
      }
    }
    
    /* highlight next item */
    else if (p & PAD_BUTTON_DOWN)
    {
      filelist[selection].filename_offset = 0;
      selection++;
      if (selection == maxfiles) selection = offset = 0;
      if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
      redraw = 1;
    }
    
    /* highlight previous item */
    else if (p & PAD_BUTTON_UP)
    {
      filelist[selection].filename_offset = 0;
      selection--;
      if (selection < 0)
      {
        selection = maxfiles - 1;
        offset = selection - PAGESIZE + 1;
      }
      if (selection < offset) offset -= PAGESIZE;
      if (offset < 0) offset = 0;
      redraw = 1;
    }
    
    /* go back one page */
    else if (p & PAD_TRIGGER_L)
    {
      filelist[selection].filename_offset = 0;
      selection -= PAGESIZE;
      if (selection < 0)
      {
        selection = maxfiles - 1;
        offset = selection - PAGESIZE + 1;
      }
      if (selection < offset) offset -= PAGESIZE;
      if (offset < 0) offset = 0;
      redraw = 1;
    }
    
    /* go forward one page */
    else if (p & PAD_TRIGGER_R)
    {
      filelist[selection].filename_offset = 0;
      selection += PAGESIZE;
      if (selection > maxfiles - 1) selection = offset = 0;
      if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
      redraw = 1;
    }
    
    /* go up one directory or quit */
    if (p & PAD_BUTTON_B)
    {
      filelist[selection].filename_offset = 0;
      if (UseSDCARD)
      {
        if (strcmp(filelist[0].filename,"..") != 0) return;
      }
      else
      {
        if (basedir == rootdir) return;
      }
      go_up = 1;
    }
    
    /* quit */
    if (p & PAD_TRIGGER_Z)
    {
      filelist[selection].filename_offset = 0;
      return;
    }
    
    /* open selected file or directory */
    if ((p & PAD_BUTTON_A) || go_up)
    {
      filelist[selection].filename_offset = 0;
      if (go_up)
      {
        /* select item #1 */
        go_up = 0;
        selection = UseSDCARD ? 0 : 1;
      }
      
      /*** This is directory ***/
      if (filelist[selection].flags)
      {
				/* SDCARD directory handler */
        if (UseSDCARD)
				{
					/* update current directory */
					if (updateSDdirname())
					{
						/* reinit selector (previous value is saved for one level) */
						if (selection == 0)
						{
							selection = old_selection;
							offset = old_offset;
							old_selection = 0;
							old_offset = 0;
						}
						else
						{
							/* save current selector value */
							old_selection = selection;
							old_offset = offset;
							selection = 0;
							offset = 0;
						}
						
						/* set new entry list */
						maxfiles = parseSDdirectory();
						if (!maxfiles)
						{
							/* quit */
							WaitPrompt ("No files found !");
							haverom   = 1;
							haveSDdir = 0;
						}
					}
				}
				else /* DVD directory handler */
				{
					/* move to a new directory */
					if (selection != 0)
					{
						/* update current directory */
						rootdir = filelist[selection].offset;
						rootdirlength = filelist[selection].length;
				  
						/* reinit selector (previous value is saved for one level) */
						if (selection == 1)
						{
							selection = old_selection;
							offset = old_offset;
							old_selection = 0;
							old_offset = 0;
						}
						else
						{
							/* save current selector value */
							old_selection = selection;
							old_offset = offset;
							selection = 0;
							offset = 0;
						}

						/* get new entry list */
						maxfiles = parseDVDdirectory ();
					}
				}
			}
			else /*** This is a file ***/
			{
				FileSelected();
			  	haverom = 1;
			}
			redraw = 1;
		}
	}
}

/****************************************************************************
 * OpenDVD
 *
 * Function to load a DVD directory and display to user.
 ****************************************************************************/ 
void OpenDVD () 
{
  UseSDCARD = 0;
  UseHistory = 0;
  
  if (!getpvd())
  {
		ShowAction("Mounting DVD ... Wait");
#ifndef HW_RVL
		DVD_Mount();
#endif
    haveDVDdir = 0;
		if (!getpvd())
		{
			WaitPrompt ("Failed to mount DVD");
      return;
    }
  }
  
  if (haveDVDdir == 0)
  {
    /* don't mess with SD entries */
    haveSDdir = 0;
    
    /* reinit selector */
    rootdir = basedir;
    old_selection = selection = offset = old_offset = 0;
    
    if ((maxfiles = parseDVDdirectory ()))
    {
	    FileSelector ();
      haveDVDdir = 1;
    }
  }
  else FileSelector ();
}
 
/****************************************************************************
 * OpenSD
 *
 * Function to load a SDCARD directory and display to user.
 ****************************************************************************/ 
int OpenSD ()
{
  UseSDCARD = 1;
  UseHistory = 0;
  
  if (haveSDdir == 0)
  {
    /* don't mess with DVD entries */
    haveDVDdir = 0;

    /* reinit selector */
    old_selection = selection = offset = old_offset = 0;

    /* Reset SDCARD root directory */
    sprintf (rootSDdir, "/genplus/roms/");

    /* if directory doesn't exist, use root */
    DIR_ITER *dir = diropen(rootSDdir);
    if (dir == NULL) sprintf (rootSDdir, "fat:/");
    else dirclose(dir);
  }
 
  /* Parse root directory and get entries list */
  ShowAction("Reading Directory ...");
  if ((maxfiles = parseSDdirectory ()))
  {
    /* Select an entry */
    FileSelector ();
		 
    /* memorize last entries list, actual root directory and selection for next access */
    haveSDdir = 1;
  }
  else
  {
    /* no entries found */
    WaitPrompt ("no files found !");
    haveSDdir = 0;
    return 0;
  }
  
  return 1;
}
  
void OpenHistory()
{
	int i;

	UseSDCARD = 1;
	UseHistory = 1;

  /* don't mess with SD entries */
  haveSDdir = 0;

  /* reinit selector */
  old_selection = selection = offset = old_offset = 0;

    /* Reset SDCARD root directory */
	/* Make this empty because the history */
	/* entry will contain the entire path. */
    /*sprintf (rootSDdir, "");*/
	
	/* Recreate the file listing from the history
     * as if all of the roms were in the same directory. */
	ShowAction("Reading Files ...");

	maxfiles = 0;
	for(i=0; i < NUM_HISTORY_ENTRIES; i++)
	{
		if(history.entries[i].filepath[0] > 0)
		{
			filelist[i].offset = 0;
			filelist[i].length = 0;
			filelist[i].flags = 0;
			filelist[i].filename_offset = 0;
			strncpy(filelist[i].filename, history.entries[i].filename, MAXJOLIET-1);
			filelist[i].filename[MAXJOLIET-1] = '\0';
			
			maxfiles++;
		}
		else
		{
			/* Found the end of the list. */
			break;
		}
	}
	
	if(!maxfiles)
	{
		WaitPrompt ("No recent files");
		return;
	}
	
	FileSelector();
}
  

/****************************************************************************
 * LoadFile
 *
 * This function will load a file from DVD or SDCARD, in BIN, SMD or ZIP format.
 * The values for offset and length are inherited from rootdir and 
 * rootdirlength.
 *
 * The buffer parameter should re-use the initial ROM buffer.
 ****************************************************************************/ 
static int LoadFile (unsigned char *buffer) 
{
  int readoffset;
  int blocks;
  int i;
  u64 discoffset = 0;
  char readbuffer[2048];
  char fname[MAXPATHLEN];
 
  /* SDCard access */ 
  if (UseSDCARD)
  {
    /* open file */
    sprintf(fname, "%s%s",rootSDdir,filelist[selection].filename);
    sdfile = fopen(fname, "rb");
    if (sdfile == NULL)
    {
      WaitPrompt ("Unable to open file!");
      haveSDdir = 0;
      return 0;
    }
  }

  /* How many 2k blocks to read */
  if (rootdirlength == 0) return 0;
  blocks = rootdirlength / 2048;
  readoffset = 0;
  
  ShowAction ("Loading ... Wait");
  
  /* Read first data chunk */
  if (UseSDCARD)
  {
    fread(readbuffer, 1, 2048, sdfile);
  }
  else
  {
    discoffset = rootdir;
    dvd_read (&readbuffer, 2048, discoffset);
  }
  
  /* determine file type */
  if (!IsZipFile ((char *) readbuffer))
  {
    /* go back to file start */
    if (UseSDCARD)
    {
      fseek(sdfile, 0, SEEK_SET);
  	}

    /* read data chunks */
    for (i = 0; i < blocks; i++)
    {
      if (UseSDCARD)
      {
        fread(readbuffer, 1, 2048, sdfile);
      }
      else
      {
        dvd_read(readbuffer, 2048, discoffset);
        discoffset += 2048;
      }

      memcpy (buffer + readoffset, readbuffer, 2048);
      readoffset += 2048;
    }

    /* final read */ 
    i = rootdirlength % 2048;
    if (i)
    {
      if (UseSDCARD) fread(readbuffer, 1, i, sdfile);
      else dvd_read (readbuffer, 2048, discoffset);
      memcpy (buffer + readoffset, readbuffer, i);
    }
  }
  else
  {
    /* unzip file */
    return UnZipBuffer (buffer, discoffset, rootdirlength, UseSDCARD);
  }
  
  /* close SD file */
  if (UseSDCARD) fclose(sdfile);

  return rootdirlength;
}
