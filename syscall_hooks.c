/* syscall_hooks.c -- System call interception implementation */

#define _GNU_SOURCE
#include "config.h"
#include "syscall_hooks.h"
#include "debug_log.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Original system call function pointers */
static int (*orig_unlink)(const char *pathname) = NULL;
static int (*orig_unlinkat)(int dirfd, const char *pathname, int flags) = NULL;
static int (*orig_close)(int fd) = NULL;
static int (*orig_dup2)(int oldfd, int newfd) = NULL;
static ssize_t (*orig_write)(int fd, const void *buf, size_t count) = NULL;
static ssize_t (*orig_read)(int fd, void *buf, size_t count) = NULL;

/* File descriptor tracking */
#define MAX_TRACKED_FDS 1024
static struct {
    int fd;
    char path[256];
    char purpose[128];
    int protected;
} tracked_fds[MAX_TRACKED_FDS];
static int tracked_fd_count = 0;

static int hooks_initialized = 0;

void
init_syscall_hooks(void)
{
    if (hooks_initialized)
        return;
        
    /* Get original function pointers */
    orig_unlink = dlsym(RTLD_NEXT, "unlink");
    orig_unlinkat = dlsym(RTLD_NEXT, "unlinkat");
    orig_close = dlsym(RTLD_NEXT, "close");
    orig_dup2 = dlsym(RTLD_NEXT, "dup2");
    orig_write = dlsym(RTLD_NEXT, "write");
    orig_read = dlsym(RTLD_NEXT, "read");
    
    if (!orig_unlink || !orig_close || !orig_write || !orig_read)
    {
        LOG_SYSCALL("dlsym", "failed to get original function pointers", -1);
        return;
    }
    
    hooks_initialized = 1;
    LOG_SYSCALL("init_hooks", "system call hooks initialized", 0);
}

void
cleanup_syscall_hooks(void)
{
    hooks_initialized = 0;
    tracked_fd_count = 0;
    LOG_SYSCALL("cleanup_hooks", "system call hooks cleaned up", 0);
}

void
track_fd_usage(int fd, const char *operation)
{
    char fd_path[256];
    char details[512];
    ssize_t len;
    
    if (tracked_fd_count >= MAX_TRACKED_FDS)
        return;
        
    /* Try to get the file path from /proc/self/fd/ */
    snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fd);
    len = readlink(fd_path, tracked_fds[tracked_fd_count].path, 
                   sizeof(tracked_fds[tracked_fd_count].path) - 1);
    
    if (len > 0)
    {
        tracked_fds[tracked_fd_count].path[len] = '\0';
    }
    else
    {
        snprintf(tracked_fds[tracked_fd_count].path, 
                sizeof(tracked_fds[tracked_fd_count].path), "unknown");
    }
    
    tracked_fds[tracked_fd_count].fd = fd;
    strncpy(tracked_fds[tracked_fd_count].purpose, operation, 
            sizeof(tracked_fds[tracked_fd_count].purpose) - 1);
    tracked_fds[tracked_fd_count].protected = 0;
    
    snprintf(details, sizeof(details), "fd=%d path=%s operation=%s", 
             fd, tracked_fds[tracked_fd_count].path, operation);
    LOG_FD_OP("TRACK", fd, details);
    
    tracked_fd_count++;
}

/* Hooked unlink - prevent deletion of tracked files */
int
hooked_unlink(const char *pathname)
{
    char args[512];
    int result;
    
    if (!hooks_initialized || !orig_unlink)
        return unlink(pathname);
        
    snprintf(args, sizeof(args), "\"%s\"", pathname ? pathname : "null");
    
    /* Check if we should prevent this deletion */
    if (pathname && (strstr(pathname, "requirement") || getenv("PREVENT_UNLINK")))
    {
        LOG_SYSCALL("unlink", args, -1);
        LOG_FILE_OP("UNLINK_BLOCKED", pathname, -1);
        errno = EPERM;
        return -1;
    }
    
    result = orig_unlink(pathname);
    LOG_SYSCALL("unlink", args, result);
    
    if (result == 0)
        LOG_FILE_OP("UNLINK_SUCCESS", pathname, -1);
        
    return result;
}

/* Hooked unlinkat - prevent deletion of tracked files */
int
hooked_unlinkat(int dirfd, const char *pathname, int flags)
{
    char args[512];
    int result;
    
    if (!hooks_initialized || !orig_unlinkat)
        return unlinkat(dirfd, pathname, flags);
        
    snprintf(args, sizeof(args), "dirfd=%d, \"%s\", flags=%d", 
             dirfd, pathname ? pathname : "null", flags);
    
    /* Check if we should prevent this deletion */
    if (pathname && (strstr(pathname, "requirement") || getenv("PREVENT_UNLINK")))
    {
        LOG_SYSCALL("unlinkat", args, -1);
        LOG_FILE_OP("UNLINKAT_BLOCKED", pathname, dirfd);
        errno = EPERM;
        return -1;
    }
    
    result = orig_unlinkat(dirfd, pathname, flags);
    LOG_SYSCALL("unlinkat", args, result);
    
    if (result == 0)
        LOG_FILE_OP("UNLINKAT_SUCCESS", pathname, dirfd);
        
    return result;
}

/* Hooked close - log and potentially prevent closing protected FDs */
int
hooked_close(int fd)
{
    char args[32];
    int result;
    int i;
    
    if (!hooks_initialized || !orig_close)
        return close(fd);
        
    snprintf(args, sizeof(args), "fd=%d", fd);
    
    /* Check if this FD is protected */
    for (i = 0; i < tracked_fd_count; i++)
    {
        if (tracked_fds[i].fd == fd && tracked_fds[i].protected)
        {
            LOG_SYSCALL("close", args, -1);
            LOG_FD_OP("CLOSE_BLOCKED", fd, "protected file descriptor");
            errno = EPERM;
            return -1;
        }
    }
    
    result = orig_close(fd);
    LOG_SYSCALL("close", args, result);
    
    return result;
}

/* Hooked dup2 - log file descriptor duplication */
int
hooked_dup2(int oldfd, int newfd)
{
    char args[64];
    int result;
    
    if (!hooks_initialized || !orig_dup2)
        return dup2(oldfd, newfd);
        
    snprintf(args, sizeof(args), "oldfd=%d, newfd=%d", oldfd, newfd);
    
    result = orig_dup2(oldfd, newfd);
    LOG_SYSCALL("dup2", args, result);
    
    if (result >= 0)
        track_fd_usage(result, "dup2");
    
    return result;
}

/* Hooked write - log all write operations */
ssize_t
hooked_write(int fd, const void *buf, size_t count)
{
    char args[128];
    ssize_t result;
    
    if (!hooks_initialized || !orig_write)
        return write(fd, buf, count);
        
    snprintf(args, sizeof(args), "fd=%d, count=%zu", fd, count);
    
    result = orig_write(fd, buf, count);
    LOG_SYSCALL("write", args, (int)result);
    
    /* Log write data for special file descriptors */
    if (fd <= 2 || getenv("LOG_ALL_WRITES"))
    {
        if (result > 0 && buf)
            log_input_read("WRITE_DATA", (const char*)buf, (int)(result > 256 ? 256 : result));
    }
    
    return result;
}

/* Hooked read - log all read operations */
ssize_t
hooked_read(int fd, void *buf, size_t count)
{
    char args[128];
    ssize_t result;
    
    if (!hooks_initialized || !orig_read)
        return read(fd, buf, count);
        
    snprintf(args, sizeof(args), "fd=%d, count=%zu", fd, count);
    
    result = orig_read(fd, buf, count);
    LOG_SYSCALL("read", args, (int)result);
    
    /* Log read data for special file descriptors */
    if (result > 0 && buf && (fd <= 2 || getenv("LOG_ALL_READS")))
    {
        log_input_read("READ_DATA", (const char*)buf, (int)(result > 256 ? 256 : result));
    }
    
    return result;
}

void
monitor_fd_operations(void)
{
    int i;
    char fd_path[256];
    struct stat st;
    
    LOG_FD_OP("MONITOR_START", -1, "Starting FD monitoring");
    
    for (i = 0; i < 256; i++)
    {
        snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", i);
        if (stat(fd_path, &st) == 0)
        {
            track_fd_usage(i, "existing");
        }
    }
    
    LOG_FD_OP("MONITOR_END", -1, "FD monitoring complete");
}

/* Override the system calls using weak symbols or LD_PRELOAD */
#ifdef USE_WEAK_SYMBOLS
int unlink(const char *pathname) __attribute__((weak, alias("hooked_unlink")));
int close(int fd) __attribute__((weak, alias("hooked_close")));
ssize_t write(int fd, const void *buf, size_t count) __attribute__((weak, alias("hooked_write")));
ssize_t read(int fd, void *buf, size_t count) __attribute__((weak, alias("hooked_read")));
#endif