# Enhanced bash-shxdumper Features

This document describes the enhanced logging and protection features added to the bash-shxdumper project.

## Enhanced Logging System

### Core Logging Features
- **Comprehensive Input Logging**: All input reads from various sources (stdin, files, scripts)
- **Command Execution Tracking**: Complete command execution with full argument capture
- **System Call Monitoring**: Detailed syscall logging with arguments and return values
- **File Operation Tracking**: File opens, reads, writes, and deletions
- **Memory Operation Logging**: Memory allocations, mappings, and deallocations
- **Process Monitoring**: Process creation, termination, and signal handling
- **File Descriptor Tracking**: Complete FD lifecycle monitoring

### Logging Configuration
Set these environment variables to enable enhanced logging:

```bash
# Enable debug logging
export LOG_FILE="/tmp/bash_debug.log"

# Enable file descriptor protection
export ENABLE_FD_PROTECTION=1

# Prevent file deletion (useful for malware analysis)
export PREVENT_UNLINK=1

# Log all read operations
export LOG_ALL_READS=1

# Log all write operations  
export LOG_ALL_WRITES=1
```

### Log Format
Each log entry includes:
- **Timestamp**: High-precision timestamp with microseconds
- **Process ID**: PID of the process generating the log
- **Process Name**: Name of the process (from /proc/self/comm)
- **Log Type**: Type of operation (READ, WRITE, SYSCALL, CMD, etc.)
- **Details**: Specific operation details

Example log entries:
```
[2025-09-15 09:18:25.612933] [12004] [bash] [INIT] Debug logging started - PID:12004 PPID:11903
[2025-09-15 09:18:25.614317] [12004] [bash] [CMD] ./requirement './requirement'
[2025-09-15 09:18:25.614324] [12004] [bash] [FILE] EXEC_ATTEMPT: ./requirement (fd:-1)
[2025-09-15 09:18:25.614334] [12004] [bash] [FD] PROTECT: fd=5 duplicated from fd:4 for requirement protection
```

## File Descriptor Protection

### Purpose
Protects critical file descriptors from being closed or deleted by malicious scripts, particularly useful for malware analysis.

### Features
- **FD Duplication**: Automatically duplicates file descriptors for important files
- **Close Prevention**: Prevents closing of protected file descriptors
- **Unlink Blocking**: Blocks deletion attempts on protected files
- **Real-time Monitoring**: Tracks all FD operations in real-time

### Usage
```bash
# Enable FD protection
export ENABLE_FD_PROTECTION=1

# Run with protection
./bash -c 'your_script_here'
```

## System Call Interception

### Intercepted Calls
- **unlink/unlinkat**: Prevents file deletion
- **close**: Protects file descriptors
- **write/read**: Logs data transfers
- **dup2**: Tracks FD duplication
- **execve**: Monitors process execution

### Hook Implementation
The system uses `dlsym(RTLD_NEXT, ...)` to intercept system calls while preserving original functionality.

## Anti-Analysis Defeat

### Techniques Handled
1. **Self-Deleting Binaries**: Prevents files from deleting themselves
2. **Memfd Execution**: Captures in-memory execution attempts
3. **File Descriptor Manipulation**: Protects against FD attacks
4. **Process Hiding**: Logs all process creation and termination

### Requirement Binary Analysis
The enhanced system successfully analyzed a sophisticated malware sample:

1. **Stage 1**: Custom UPX-encrypted ELF binary (1MB)
2. **Stage 2**: In-memory UPX decompressor (3,299 bytes)
3. **Stage 3**: Final payload decompression (2MB+ script)

### Captured Techniques
- `memfd_create("upX", MFD_EXEC)` for fileless execution
- Two-stage decompression process
- Self-deletion attempts (blocked)
- Custom UPX variant (non-standard)

## Installation and Usage

### Building Enhanced Bash
```bash
./configure
make clean
make -j$(nproc)
```

### Using for Analysis
```bash
# Set up environment
export LOG_FILE="/tmp/analysis.log"
export ENABLE_FD_PROTECTION=1
export PREVENT_UNLINK=1
export LOG_ALL_READS=1
export LOG_ALL_WRITES=1

# Run suspicious script
./bash -c './suspicious_script'

# Analyze logs
cat /tmp/analysis.log
```

### Strace Integration
For maximum visibility, combine with strace:

```bash
strace -f -e trace=all -s 8192 -o /tmp/strace.log \
  ./bash -c './suspicious_script'
```

## Files Modified

### Core Files
- `debug_log.h` - Enhanced logging interface
- `debug_log.c` - Comprehensive logging implementation
- `syscall_hooks.h` - System call interception interface
- `syscall_hooks.c` - Hook implementations
- `shell.c` - Integration with main shell
- `execute_cmd.c` - Command execution logging
- `input.c` - Input stream logging (existing)

### Build System
- `Makefile.in` - Updated to include new source files

## Performance Impact

The enhanced logging system is designed to minimize performance impact:
- **Conditional Logging**: Only active when environment variables are set
- **Efficient I/O**: Line-buffered output for real-time viewing
- **Selective Hooks**: Only essential system calls are intercepted
- **Memory Efficient**: Minimal memory overhead

## Security Considerations

- **Log File Permissions**: Ensure log files are properly secured
- **Resource Limits**: Monitor disk space when logging extensively
- **Process Isolation**: Run suspicious code in isolated environments
- **Network Monitoring**: Consider network-level monitoring for complete analysis

## Future Enhancements

Potential areas for improvement:
- **Network Call Logging**: Track network connections and data transfers
- **Crypto Operation Detection**: Identify encryption/decryption operations
- **Behavioral Analysis**: Pattern detection in logged operations
- **Automated Reporting**: Generate analysis reports automatically
- **GUI Interface**: Web-based log analysis interface