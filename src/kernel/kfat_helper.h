#ifndef _KERNEL_FAT_H_
#define _KERNEL_FAT_H_
#include "./kernel.h"

// File modes
#define F_READ 0
#define F_WRITE 1
#define F_APPEND 2

// Seek modes
#define F_SEEK_SET 0
#define F_SEEK_CUR 1
#define F_SEEK_END 2

// Error codes
#define FD_INVALID -1
#define FD_PERM_DENIED -2
#define FD_NOT_FOUND -3

/**
 * @brief Opens a file with the specified mode (read, write, or append).
 * 
 * If the file is already open and permissions allow, returns the existing FD.
 * Creates the file if it does not exist and mode is F_WRITE.
 * 
 * @param fname Name of the file to open.
 * @param mode Access mode: F_READ, F_WRITE, or F_APPEND.
 * @return File descriptor on success, or error code on failure.
 */
int k_open(const char* fname, int mode);

/**
 * @brief Closes a file descriptor.
 * 
 * Decrements the reference count and deallocates if ref_count becomes 0.
 * 
 * @param fd File descriptor.
 * @return 0 on success, or error code.
 */
int k_close(int fd);

/**
 * @brief Writes data to an open file descriptor.
 * 
 * Supports automatic block allocation and append mode.
 * 
 * @param fd File descriptor.
 * @param buf Buffer to write from.
 * @param n Number of bytes to write.
 * @return Number of bytes written, or error code.
 */
int k_write(int fd, const char* str, int n);


/**
 * @brief Reads data from an open file descriptor.
 * 
 * Handles reading from both regular files and stdin. Updates internal file offset.
 * 
 * @param fd File descriptor.
 * @param n Number of bytes to read.
 * @param buf Buffer to read into.
 * @return Number of bytes read, or error code.
 */
int k_read(int fd, int n, char* buf);

/**
 * @brief Deletes a file from the file system.
 * 
 * If the file is currently open, marks it as deleted but retains FAT chain until close.
 * 
 * @param fname Name of the file to delete.
 * @return 0 on success, or error code.
 */
int k_unlink(const char* fname);

/**
 * @brief Moves the file descriptor's offset to a new location.
 * 
 * Supports SEEK_SET, SEEK_CUR, SEEK_END.
 * 
 * @param fd File descriptor.
 * @param offset Offset amount.
 * @param whence Base for offset (F_SEEK_SET, F_SEEK_CUR, F_SEEK_END).
 * @return New offset value, or error code.
 */
int k_lseek(int fd, int offset, int whence);

/**
 * @brief Gets the permission bits of a file.
 * 
 * @param fname Name of the file.
 * @return Permission bits (0–7) or FILE_NOT_FOUND.
 */
int k_perm(const char* fname);


/**
 * @brief Lists information about a file or the entire directory.
 * 
 * If filename is provided, displays info for that file.
 * Otherwise, lists all non-deleted files in the root directory.
 * 
 * @param filename Optional name of the file to list.
 * @return 0 on success, or error code.
 */
int k_ls(const char* filename);


/**
 * @brief Concatenates and displays contents of files or standard input.
 *
 * Supports reading from one or more files and writing to either standard output
 * or a specified file (using `>` or `>>` for overwrite/append modes respectively).
 * If no input files are provided, it reads from standard input.
 *
 * Prevents reading from and appending to the same file simultaneously to avoid
 * undefined behavior.
 *
 * @param argc Number of arguments (including the command itself).
 * @param argv Argument array containing file names and optional flags:
 *             - `>` or `-w` followed by output file (overwrite)
 *             - `>>` or `-a` followed by output file (append)
 * @return 0 on success, or an appropriate error code (e.g., FS_NOT_MOUNTED, INVALID_MODE).
 */
int k_cat(int argc, char* argv[]);

/**
 * @brief Returns the size of a file in bytes.
 * 
 * Checks both open files and root directory entries.
 * 
 * @param filename Name of the file.
 * @return Size in bytes, or FILE_NOT_FOUND.
 */
int k_file_size(const char* filename);


#endif