/* debug_log.h -- Enhanced debug logging for bash input reading and execution */

#ifndef _DEBUG_LOG_H_
#define _DEBUG_LOG_H_

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

extern FILE *debug_log_file;
extern int debug_log_enabled;
extern int fd_protection_enabled;

void init_debug_log(void);
void close_debug_log(void);
void log_input_read(const char *source, const char *data, int len);
void log_script_execution(const char *script_path);
void log_command_execution(const char *command, char **argv);
void log_system_call(const char *syscall, const char *args, int result);
void log_file_operation(const char *operation, const char *path, int fd);
void log_memory_operation(const char *operation, void *ptr, size_t size);
void log_process_info(const char *event, pid_t pid);
void log_fd_operation(const char *operation, int fd, const char *details);
void log_signal_event(const char *event, int signum, const char *details);

/* Enhanced logging macros */
#define LOG_INPUT(source, data, len) \
  do { if (debug_log_enabled) log_input_read(source, data, len); } while(0)

#define LOG_COMMAND(command, argv) \
  do { if (debug_log_enabled) log_command_execution(command, argv); } while(0)

#define LOG_SYSCALL(syscall, args, result) \
  do { if (debug_log_enabled) log_system_call(syscall, args, result); } while(0)

#define LOG_FILE_OP(operation, path, fd) \
  do { if (debug_log_enabled) log_file_operation(operation, path, fd); } while(0)

#define LOG_MEMORY_OP(operation, ptr, size) \
  do { if (debug_log_enabled) log_memory_operation(operation, ptr, size); } while(0)

#define LOG_PROCESS(event, pid) \
  do { if (debug_log_enabled) log_process_info(event, pid); } while(0)

#define LOG_FD_OP(operation, fd, details) \
  do { if (debug_log_enabled) log_fd_operation(operation, fd, details); } while(0)

#define LOG_SIGNAL(event, signum, details) \
  do { if (debug_log_enabled) log_signal_event(event, signum, details); } while(0)

/* File descriptor protection functions */
int protect_fd(int fd, const char *purpose);
int duplicate_and_protect_fd(int fd, const char *purpose);
void enable_fd_protection(void);
void disable_fd_protection(void);

#endif /* _DEBUG_LOG_H_ */