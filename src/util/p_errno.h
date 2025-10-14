#ifndef P_ERRNO_H
#define P_ERRNO_H

extern int P_ERRNO;  // Global error code variable

// Define PennOS error codes
#define P_EPERM    1   // Operation not permitted
#define P_ENOENT   2   // No such file or directory
#define P_ESRCH    3   // No such process
#define P_EINTR    4   // Interrupted system call
#define P_EIO      5   // I/O error
#define P_ENOMEM   6   // Out of memory
#define P_EINVAL   7   // Invalid argument
#define P_EFORK    8   // Failed to fork
#define P_EWAITPID_I 9   // Failed to waitpid
#define P_EWAITPID_II 10   // Failed to waitpid
#define P_EWAITPID_III 11 // Failed to waitpid
#define P_EINVAL_NICE 12 // Invalid argument nice

#define FD_INVALID -1
#define FS_NOT_MOUNTED -2
#define FILE_NOT_FOUND -3
#define INVALID_MODE -4
#define PERMISSION_DENIED -5
#define DISK_FULL -6
#define TOO_MANY_OPEN_FILES -7
#define FILE_IN_USE -8
#define INVALID_WHENCE -9
#define INVALID_FAT_CONFIG -10
#define FILE_EXISTS -11
#define FS_IO_ERROR -12
#define FS_MEMORY_ERROR -13
#define FILENAME_INVALID -14
#define FD_TABLE_NULL -15
// Add more error codes relevant to YOUR system calls

// Function to print user error messages
void u_perror(const char *user_message);

#endif // P_ERRNO_H
