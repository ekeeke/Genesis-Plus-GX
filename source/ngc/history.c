/*
 *  history.c
 *  genplusgx-mdisibio
 *
 *  Created by Martin Disibio on 6/17/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#include "shared.h"
#include "font.h"
#include "history.h"

t_history history;

void history_save()
{
  /* first check if directory exist */
  DIR_ITER *dir = diropen("/genplus");
  if (dir == NULL) mkdir("/genplus",S_IRWXU);
  else dirclose(dir);

  /* open file for writing */
  FILE *fp = fopen("/genplus/romhistory.ini", "wb");
  if (fp == NULL) return;

  /* save options */
  fwrite(&history, sizeof(history), 1, fp);

  fclose(fp);
}

/****************************************************************************
 * history_add_file
 *
 * Adds the given file path to the top of the history list, shifting each
 * existing entry in the history down one place. If given file path is 
 * already in the list then the existing entry is (in effect) moved to the
 * top instead.
  ****************************************************************************/ 
void history_add_file(char *filepath, char *filename)
{
	/* Create the new entry for this path. */
	t_history_entry newentry;
	strncpy(newentry.filepath, filepath, MAXJOLIET - 1);
	strncpy(newentry.filename, filename, MAXJOLIET - 1);
	newentry.filepath[MAXJOLIET - 1] = '\0';
	newentry.filename[MAXJOLIET - 1] = '\0';
	
	t_history_entry oldentry;	/* Old entry is the one being shuffled down a spot. */	
	t_history_entry currentry;	/* Curr entry is the one that just replaced old path. */
	
	/* Initially set curr entry to the new value. */
	memcpy(&currentry, &newentry, sizeof(t_history_entry));

	int i;
	for(i=0; i < NUM_HISTORY_ENTRIES; i++)
	{	
		/* Save off the next entry. */
		memcpy(&oldentry, &history.entries[i], sizeof(t_history_entry));
		
		/* Overwrite with the previous entry. */
		memcpy(&history.entries[i], &currentry, sizeof(t_history_entry));
		
		/* Switch the old entry to the curr entry now. */
		memcpy(&currentry, &oldentry, sizeof(t_history_entry));
		
		/* If the entry in the list at this spot matches
		   the new entry then do nothing and let this
		   entry get deleted. */
		if(strcmp(newentry.filepath, currentry.filepath) == 0 && strcmp(newentry.filename, currentry.filename) == 0)
			break;
	}

	/* now save to disk */
	history_save();
}

void history_load()
{
  /* open file for reading */
  FILE *fp = fopen("/genplus/romhistory.ini", "rb");
  if (fp == NULL) return;

  /* read file */
  fread(&history, sizeof(history), 1, fp);

  fclose(fp);
}

void set_history_defaults(void)
{
	int i;
	for(i=0; i < NUM_HISTORY_ENTRIES; i++)
	{
		memset(&history.entries[i], 0, sizeof(t_history_entry));
	}
}



