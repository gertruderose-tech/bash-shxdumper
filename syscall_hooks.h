/* syscall_hooks.h -- System call interception for enhanced logging */

#ifndef _SYSCALL_HOOKS_H_
#define _SYSCALL_HOOKS_H_

#include <sys/types.h>
#include <unistd.h>

/* Hook system calls to prevent self-deletion and log activity */
void init_syscall_hooks(void);
void cleanup_syscall_hooks(void);

/* Hooked system calls */
int hooked_unlink(const char *pathname);
int hooked_unlinkat(int dirfd, const char *pathname, int flags);
int hooked_close(int fd);
int hooked_dup2(int oldfd, int newfd);
ssize_t hooked_write(int fd, const void *buf, size_t count);
ssize_t hooked_read(int fd, void *buf, size_t count);

/* File descriptor monitoring */
void monitor_fd_operations(void);
void track_fd_usage(int fd, const char *operation);

#endif /* _SYSCALL_HOOKS_H_ */