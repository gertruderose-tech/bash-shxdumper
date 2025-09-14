/* debug_log.h -- Debug logging for bash input reading */

#ifndef _DEBUG_LOG_H_
#define _DEBUG_LOG_H_

#include <stdio.h>
#include <unistd.h>
#include <time.h>

extern FILE *debug_log_file;
extern int debug_log_enabled;

void init_debug_log(void);
void close_debug_log(void);
void log_input_read(const char *source, const char *data, int len);
void log_script_execution(const char *script_path);

#define LOG_INPUT(source, data, len) \
  do { if (debug_log_enabled) log_input_read(source, data, len); } while(0)

#endif /* _DEBUG_LOG_H_ */