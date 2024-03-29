#ifndef __CORETYPES_H__
#define __CORETYPES_H__

#include <stdint.h>
#include <stdio.h>
#include "../../../macros.h"

#define ARRAY_LENGTH(x) (sizeof(x)/sizeof(x[0]))

#define core_file                 cdStream
#define core_fopen                cdStreamOpen
#define core_fseek                cdStreamSeek
#define core_fread(fc, buff, len) cdStreamRead(buff, 1, len, fc)
#define core_fclose               cdStreamClose
#define core_ftell                cdStreamTell

#endif
