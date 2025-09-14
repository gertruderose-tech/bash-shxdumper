/* debug_log.c -- Debug logging implementation */

#include "config.h"
#include "debug_log.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

FILE *debug_log_file = NULL;
int debug_log_enabled = 0;

void
init_debug_log(void)
{
  char *log_file_path;
  
  log_file_path = getenv("LOG_FILE");
  if (!log_file_path || !*log_file_path)
    log_file_path = "/tmp/bash_debug.log";
  
  debug_log_file = fopen(log_file_path, "a");
  if (debug_log_file)
    {
      debug_log_enabled = 1;
    }
}

void
close_debug_log(void)
{
  if (debug_log_file)
    {
      fclose(debug_log_file);
      debug_log_file = NULL;
      debug_log_enabled = 0;
    }
}

void
log_input_read(const char *source, const char *data, int len)
{
  int i;
  
  if (!debug_log_enabled || !debug_log_file)
    return;
    
  fprintf(debug_log_file, "[%d] [%s] READ %d bytes: ", getpid(), source, len);
  
  for (i = 0; i < len; i++)
    {
      if (data[i] == '\n')
        fprintf(debug_log_file, "\\n");
      else if (data[i] == '\t')
        fprintf(debug_log_file, "\\t");
      else if (data[i] == '\r')
        fprintf(debug_log_file, "\\r");
      else if (data[i] >= 32 && data[i] <= 126)
        fputc(data[i], debug_log_file);
      else
        fprintf(debug_log_file, "\\x%02x", (unsigned char)data[i]);
    }
  
  fprintf(debug_log_file, "\n");
  fflush(debug_log_file);
}

void
log_script_execution(const char *script_path)
{
  if (!debug_log_enabled || !debug_log_file)
    return;
    
  fprintf(debug_log_file, "[%d] [INIT] [%s]\n", getpid(), script_path ? script_path : "unknown");
  fflush(debug_log_file);
}