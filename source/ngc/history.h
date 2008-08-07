/*
 *  history.h
 *  genplusgx-mdisibio
 *
 *  Created by Martin Disibio on 6/17/08.
 *
 */
 #ifndef _HISTORY_H
 #define _HISTORY_H
 
 #include "types.h"
 #include "iso9660.h"
 
 
 /****************************************************************************
 * ROM Play History
 *
 ****************************************************************************/ 
 #define NUM_HISTORY_ENTRIES	(10)
 
 
typedef struct 
{
  char filepath[MAXJOLIET];
  char filename[MAXJOLIET];
} t_history_entry;

typedef struct
{
	t_history_entry entries[NUM_HISTORY_ENTRIES];
} t_history;

extern t_history history;
extern void history_add_file(char *filepath, char *filename);
extern void history_load();
extern void set_history_defaults();
 
 
 #endif

