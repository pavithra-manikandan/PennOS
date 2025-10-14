#include "p_errno.h"
#include "./syscall/sys_call.h"
#include <stdio.h>

int P_ERRNO = 0;  // Start with no error

void u_perror(const char *user_message) {
    const char *error_message;
    char msg[PRINT_BUFFER_SIZE];

    switch (P_ERRNO) {
        case P_EPERM:
            error_message = "Operation not permitted";
            break;
        case P_ENOENT:
            error_message = "No such file or directory";
            break;
        case P_ESRCH:
            error_message = "No such process";
            break;
        case P_EINTR:
            error_message = "Interrupted system call";
            break;
        case P_EIO:
            error_message = "I/O error";
            break;
        case P_ENOMEM:
            error_message = "Out of memory";
            break;
        case P_EINVAL:
            error_message = "Invalid argument";
            break;
        case P_EFORK:
            error_message = "Failed to fork";
            break;
        case P_EWAITPID_I:
            error_message = "Failed to waitpid : parent is NULL";
            break;
        case P_EWAITPID_II:
            error_message = "Failed to waitpid : no children to wait on";
            break;
        case P_EWAITPID_III:
            error_message = "Failed to waitpid";
            break;
        case P_EINVAL_NICE:
            error_message = "Priority must be between 0 and 2";
            break;
        case FD_INVALID:
            error_message = "Invalid file descriptor";
            break;
        case FS_NOT_MOUNTED:
            error_message = "File system not mounted";
            break;
        case FILE_NOT_FOUND:
            error_message = "File not found";
            break;
        case INVALID_MODE:
            error_message = "Invalid mode specified";
            break;
        case PERMISSION_DENIED:
            error_message = "Permission denied";
            break;
        case DISK_FULL:
            error_message = "Disk full";
            break;
        case TOO_MANY_OPEN_FILES:
            error_message = "Too many open files";
            break;
        case FILE_IN_USE:
            error_message = "File in use";
            break;
        case INVALID_WHENCE:
            error_message = "Invalid 'whence' argument for seek";
            break;
        case INVALID_FAT_CONFIG:
            error_message = "Invalid FAT file system configuration";
            break;
        case FILE_EXISTS:
            error_message = "File already exists";
            break;
        case FS_IO_ERROR:
            error_message = "File system I/O error";
            break;
        case FS_MEMORY_ERROR:
            error_message = "File system memory allocation error";
            break;
        case FILENAME_INVALID:
            error_message = "Invalid filename";
            break;
        case FD_TABLE_NULL:
            error_message = "fd_table is NULL";
            break;
        default:
            error_message = "Unknown error";
    }

    int len;
    if (user_message) {
        len = snprintf(msg, sizeof(msg), "%s: %s\n", user_message, error_message);
    } else {
        len = snprintf(msg, sizeof(msg), "%s\n", error_message);
    }
    s_write(STDERR_FILENO, len, msg);
}
