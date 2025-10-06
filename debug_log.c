/* debug_log.c -- Enhanced debug logging implementation */

#include "config.h"
#include "debug_log.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

FILE *debug_log_file = NULL;
int debug_log_enabled = 0;
int fd_protection_enabled = 0;

/* Array to track protected file descriptors */
static int protected_fds[256];
static int protected_fd_count = 0;

static void log_timestamp(void);
static const char* get_process_name(void);

void
init_debug_log(void)
{
  char *log_file_path;
  char *enable_protection;
  
  log_file_path = getenv("LOG_FILE");
  if (!log_file_path || !*log_file_path)
    log_file_path = "/tmp/bash_debug.log";
  
  debug_log_file = fopen(log_file_path, "a");
  if (debug_log_file)
    {
      debug_log_enabled = 1;
      setvbuf(debug_log_file, NULL, _IOLBF, 0); /* Line buffered for real-time viewing */
      
      /* Check if FD protection should be enabled */
      enable_protection = getenv("ENABLE_FD_PROTECTION");
      if (enable_protection && *enable_protection)
        {
          fd_protection_enabled = 1;
          fprintf(debug_log_file, "[%d] [INIT] File descriptor protection ENABLED\n", getpid());
        }
      
      log_timestamp();
      fprintf(debug_log_file, "[%d] [%s] [INIT] Debug logging started - PID:%d PPID:%d\n", 
              getpid(), get_process_name(), getpid(), getppid());
      fflush(debug_log_file);
    }
}

void
close_debug_log(void)
{
  if (debug_log_file)
    {
      log_timestamp();
      fprintf(debug_log_file, "[%d] [%s] [EXIT] Debug logging ended\n", getpid(), get_process_name());
      fflush(debug_log_file);
      fclose(debug_log_file);
      debug_log_file = NULL;
      debug_log_enabled = 0;
    }
}

static void
log_timestamp(void)
{
  struct timeval tv;
  struct tm *tm_info;
  char time_buffer[64];
  
  if (!debug_log_enabled || !debug_log_file)
    return;
    
  gettimeofday(&tv, NULL);
  tm_info = localtime(&tv.tv_sec);
  strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);
  fprintf(debug_log_file, "[%s.%06ld] ", time_buffer, tv.tv_usec);
}

static const char*
get_process_name(void)
{
  static char proc_name[256];
  static int cached = 0;
  FILE *f;
  
  if (cached)
    return proc_name;
    
  f = fopen("/proc/self/comm", "r");
  if (f)
    {
      if (fgets(proc_name, sizeof(proc_name), f) != NULL)
        {
          char *newline = strchr(proc_name, '\n');
          if (newline) *newline = '\0';
          cached = 1;
        }
      fclose(f);
    }
  
  if (!cached)
    {
      strncpy(proc_name, "unknown", sizeof(proc_name) - 1);
      proc_name[sizeof(proc_name) - 1] = '\0';
      cached = 1;
    }
  
  return proc_name;
}

void
log_input_read(const char *source, const char *data, int len)
{
  int i;
  
  if (!debug_log_enabled || !debug_log_file)
    return;
    
  log_timestamp();
  fprintf(debug_log_file, "[%d] [%s] [READ] Source:%s Bytes:%d Data: ", 
          getpid(), get_process_name(), source, len);
  
  for (i = 0; i < len && i < 1024; i++) /* Limit to prevent huge dumps */
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
  
  if (len > 1024)
    fprintf(debug_log_file, "... [truncated %d bytes]", len - 1024);
  
  fprintf(debug_log_file, "\n");
  fflush(debug_log_file);
}

void
log_script_execution(const char *script_path)
{
  if (!debug_log_enabled || !debug_log_file)
    return;
    
  log_timestamp();
  fprintf(debug_log_file, "[%d] [%s] [EXEC] Script: %s\n", 
          getpid(), get_process_name(), script_path ? script_path : "unknown");
  fflush(debug_log_file);
}

void
log_command_execution(const char *command, char **argv)
{
  int i;
  
  if (!debug_log_enabled || !debug_log_file)
    return;
    
  log_timestamp();
  fprintf(debug_log_file, "[%d] [%s] [CMD] %s", getpid(), get_process_name(), command ? command : "null");
  
  if (argv)
    {
      for (i = 0; argv[i] != NULL && i < 32; i++) /* Limit args to prevent huge dumps */
        {
          fprintf(debug_log_file, " '%s'", argv[i]);
        }
    }
  
  fprintf(debug_log_file, "\n");
  fflush(debug_log_file);
}

void
log_system_call(const char *syscall, const char *args, int result)
{
  if (!debug_log_enabled || !debug_log_file)
    return;
    
  log_timestamp();
  fprintf(debug_log_file, "[%d] [%s] [SYSCALL] %s(%s) = %d", 
          getpid(), get_process_name(), syscall, args ? args : "", result);
  
  if (result < 0)
    fprintf(debug_log_file, " [errno:%d %s]", errno, strerror(errno));
  
  fprintf(debug_log_file, "\n");
  fflush(debug_log_file);
}

void
log_file_operation(const char *operation, const char *path, int fd)
{
  if (!debug_log_enabled || !debug_log_file)
    return;
    
  log_timestamp();
  fprintf(debug_log_file, "[%d] [%s] [FILE] %s: %s (fd:%d)\n", 
          getpid(), get_process_name(), operation, path ? path : "null", fd);
  fflush(debug_log_file);
}

void
log_memory_operation(const char *operation, void *ptr, size_t size)
{
  if (!debug_log_enabled || !debug_log_file)
    return;
    
  log_timestamp();
  fprintf(debug_log_file, "[%d] [%s] [MEMORY] %s: ptr=%p size=%zu\n", 
          getpid(), get_process_name(), operation, ptr, size);
  fflush(debug_log_file);
}

void
log_process_info(const char *event, pid_t pid)
{
  if (!debug_log_enabled || !debug_log_file)
    return;
    
  log_timestamp();
  fprintf(debug_log_file, "[%d] [%s] [PROCESS] %s: pid=%d\n", 
          getpid(), get_process_name(), event, pid);
  fflush(debug_log_file);
}

void
log_fd_operation(const char *operation, int fd, const char *details)
{
  if (!debug_log_enabled || !debug_log_file)
    return;
    
  log_timestamp();
  fprintf(debug_log_file, "[%d] [%s] [FD] %s: fd=%d %s\n", 
          getpid(), get_process_name(), operation, fd, details ? details : "");
  fflush(debug_log_file);
}

void
log_signal_event(const char *event, int signum, const char *details)
{
  if (!debug_log_enabled || !debug_log_file)
    return;
    
  log_timestamp();
  fprintf(debug_log_file, "[%d] [%s] [SIGNAL] %s: signal=%d %s\n", 
          getpid(), get_process_name(), event, signum, details ? details : "");
  fflush(debug_log_file);
}

/* File descriptor protection functions */
void
enable_fd_protection(void)
{
  fd_protection_enabled = 1;
  LOG_FD_OP("ENABLE_PROTECTION", -1, "File descriptor protection enabled");
}

void
disable_fd_protection(void)
{
  fd_protection_enabled = 0;
  LOG_FD_OP("DISABLE_PROTECTION", -1, "File descriptor protection disabled");
}

int
protect_fd(int fd, const char *purpose)
{
  int flags;
  
  if (fd < 0 || protected_fd_count >= 255)
    return -1;
    
  /* Remove close-on-exec flag if set */
  flags = fcntl(fd, F_GETFD);
  if (flags != -1)
    {
      flags &= ~FD_CLOEXEC;
      fcntl(fd, F_SETFD, flags);
    }
  
  protected_fds[protected_fd_count++] = fd;
  
  LOG_FD_OP("PROTECT", fd, purpose ? purpose : "general protection");
  return fd;
}

int
duplicate_and_protect_fd(int fd, const char *purpose)
{
  int new_fd;
  char details[256];
  char *tmp_file_path;
  
  if (fd < 0)
    return -1;
    
  new_fd = dup(fd);
  if (new_fd >= 0)
    {
      snprintf(details, sizeof(details), "duplicated from fd:%d for %s", fd, purpose ? purpose : "protection");
      protect_fd(new_fd, details);
      LOG_FD_OP("DUPLICATE", new_fd, details);
      
      /* Save duplicated FD contents to a temporary file */
      tmp_file_path = duplicate_fd_to_file(new_fd, purpose);
      if (tmp_file_path)
        {
          char formatted_details[512];
          snprintf(formatted_details, sizeof(formatted_details), "fd:%d -> %s", fd, tmp_file_path);
          LOG_FD_OP("DUPLICATE_TO_FILE", -1, formatted_details);
          free(tmp_file_path);
        }
    }
  else
    {
      snprintf(details, sizeof(details), "failed to duplicate fd:%d - %s", fd, strerror(errno));
      LOG_FD_OP("DUPLICATE_FAILED", fd, details);
    }
  
  return new_fd;
}

void
log_fd_contents(int fd, const char *prefix)
{
  char buffer[8192];
  ssize_t bytes_read;
  off_t original_pos;
  
  if (!debug_log_enabled || !debug_log_file || fd < 0)
    {
      LOG_FD_OP("CONTENTS_SKIPPED", fd, "debug not enabled or invalid fd");
      return;
    }
  
  /* Save current file position */
  original_pos = lseek(fd, 0, SEEK_CUR);
  if (original_pos == -1)
    {
      LOG_FD_OP("CONTENTS_SKIPPED", fd, "failed to get current position");
      return;
    }
  
  /* Seek to beginning */
  if (lseek(fd, 0, SEEK_SET) == -1)
    {
      LOG_FD_OP("CONTENTS_SKIPPED", fd, "failed to seek to beginning");
      return;
    }
  
  log_timestamp();
  fprintf(debug_log_file, "[%d] [%s] [FD] CONTENTS: ( ", getpid(), get_process_name());
  
  /* Read and log contents */
  bytes_read = read(fd, buffer, sizeof(buffer));
  if (bytes_read > 0)
    {
      int i;
      /* Limit output to first 1024 bytes to prevent huge logs */
      int output_limit = (bytes_read > 1024) ? 1024 : bytes_read;
      for (i = 0; i < output_limit; i++)
        {
          if (buffer[i] == '\n')
            fprintf(debug_log_file, "\\n");
          else if (buffer[i] == '\t')
            fprintf(debug_log_file, "\\t");
          else if (buffer[i] == '\r')
            fprintf(debug_log_file, "\\r");
          else if (buffer[i] >= 32 && buffer[i] <= 126)
            fputc(buffer[i], debug_log_file);
          else
            fprintf(debug_log_file, "\\x%02x", (unsigned char)buffer[i]);
        }
      if (bytes_read > 1024)
        fprintf(debug_log_file, "... [truncated %zd bytes]", bytes_read - 1024);
    }
  else
    {
      fprintf(debug_log_file, "[no content or read failed]");
    }
  
  fprintf(debug_log_file, " )\n");
  fflush(debug_log_file);
  
  /* Restore original file position */
  lseek(fd, original_pos, SEEK_SET);
}

char*
duplicate_fd_to_file(int fd, const char *purpose)
{
  char *tmp_path;
  int tmp_fd;
  char buffer[8192];
  ssize_t bytes_read, bytes_written;
  off_t original_pos;
  
  if (fd < 0)
    return NULL;
  
  /* Create temporary file path */
  tmp_path = malloc(256);
  if (!tmp_path)
    return NULL;
  
  snprintf(tmp_path, 256, "/tmp/fd_contents.tmp");
  
  /* Save current file position */
  original_pos = lseek(fd, 0, SEEK_CUR);
  if (original_pos == -1)
    {
      free(tmp_path);
      return NULL;
    }
  
  /* Seek to beginning */
  if (lseek(fd, 0, SEEK_SET) == -1)
    {
      free(tmp_path);
      return NULL;
    }
  
  /* Create temporary file */
  tmp_fd = open(tmp_path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
  if (tmp_fd < 0)
    {
      free(tmp_path);
      lseek(fd, original_pos, SEEK_SET);
      return NULL;
    }
  
  /* Copy contents */
  while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
    {
      bytes_written = write(tmp_fd, buffer, bytes_read);
      if (bytes_written != bytes_read)
        {
          close(tmp_fd);
          unlink(tmp_path);
          free(tmp_path);
          lseek(fd, original_pos, SEEK_SET);
          return NULL;
        }
    }
  
  close(tmp_fd);
  
  /* Restore original file position */
  lseek(fd, original_pos, SEEK_SET);
  
  /* Just return the path, the calling function will format it properly */
  char *result = strdup(tmp_path);
  
  free(tmp_path);
  return result;
}