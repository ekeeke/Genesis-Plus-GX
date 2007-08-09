/*
    error.c --
    Error logging 
*/

#include "shared.h"

FILE *error_log;

struct {
    int enabled;
    int verbose;
    FILE *log;
} t_error;

void error_init(void)
{
#ifdef DEBUG    
    error_log = fopen("error.log","w");
#endif
}

void error_shutdown(void)
{
#ifdef DEBUG    
    if(error_log) fclose(error_log);
#endif
}

void error(char *format, ...)
{
#ifdef DEBUG    
    va_list ap;
    va_start(ap, format);
    if(error_log) vfprintf(error_log, format, ap);
    va_end(ap);
#endif
}

