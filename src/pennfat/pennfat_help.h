#ifndef PENNFAT_H
#define PENNFAT_H

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

// Constants
#define MAX_FILENAME_LEN 32
#define MAX_ROOT_DIR_BLOCKS 64
#define DIR_ENTRY_SIZE 64
#define FAT_ENTRY_FREE 0
#define FAT_ENTRY_LAST 0xFFFF
#define FAT_MAX_BLOCKS 32
#define FAT_BLOCK_SIZES {256, 512, 1024, 2048, 4096}

// File types
#define FT_UNKNOWN 0
#define FT_REGULAR 1
#define FT_DIRECTORY 2
#define FT_SYMLINK 4

// Permissions
#define PERM_NONE 0
#define PERM_WRITE 2
#define PERM_READ 4
#define PERM_READ_EXEC 5
#define PERM_READ_WRITE 6
#define PERM_EXEC 1
#define PERM_ALL 7

// File modes for k_open
#define F_READ 0
#define F_WRITE 1
#define F_APPEND 2

// Seek modes for k_lseek
#define F_SEEK_SET 0
#define F_SEEK_CUR 1
#define F_SEEK_END 2

// Directory entry markers
#define DIR_ENTRY_END 0
#define DIR_ENTRY_DELETED 1
#define DIR_ENTRY_IN_USE 2

// System limits
#define MAX_OPEN_FILES 32
#define MAX_ROOT_ENTRIES 512  // Adjust based on your block size/entry size
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// FAT Directory Entry
typedef struct {
  char name[MAX_FILENAME_LEN];
  uint32_t size;
  uint16_t first_block;
  uint8_t type;
  uint8_t perm;
  time_t mtime;
  char reserved[16];
} dir_entry_t;

// File descriptor entry
typedef struct {
  int fd;
  uint16_t current_block;
  uint32_t offset;
  int mode;
  int ref_count;
  dir_entry_t* entry;
} file_descriptor_t;

// Process-specific file descriptor table entry
typedef struct proc_fd_ent_st {
  int proc_fd;  // Process specific file descriptor
  int mode;     // file mode of ref'd process: F_WRITE, F_READ, F_APPEND
  int offset;
  int global_fd;  // Global file descriptor
} proc_fd_ent;

// Filesystem State
typedef struct {
  int fs_fd;                                     // Disk image file descriptor
  uint16_t* fat;                                 // Memory-mapped FAT
  uint16_t fat_blocks;                           // Number of blocks in FAT
  uint16_t block_size;                           // Block size in bytes
  uint32_t fat_size;                             // FAT size in bytes
  uint32_t data_start;                           // Offset to data region
  dir_entry_t* root_dir;                         // Root directory entries
  int is_mounted;                                // Mount status flag
  file_descriptor_t open_files[MAX_OPEN_FILES];  // Open files table
  int root_dir_blocks;
} pennfat_state_t;

extern pennfat_state_t state;


void k_print(const char* fmt, ...);

/**
 * @brief Find the first free FAT entry.
 * 
 * @return Index of the free FAT entry, or 0 if none found.
 */
uint16_t find_free_fat_entry();

/**
 * @brief Find a directory entry by its name.
 * 
 * @param name Pointer to name of the file to search for.
 * @return Pointer to the directory entry if found, NULL otherwise.
 */
dir_entry_t* find_dir_entry(const char* name);



/**
 * @brief Write the root directory blocks to disk.
 * 
 * @param entry Optional pointer for syncing particular blocks.
 */
void sync_directory_entry(dir_entry_t* entry);

/**
 * @brief Format and initialize a new PennFAT filesystem.
 *
 * This function creates a new filesystem with the specified name,
 * FAT block count, and block size configuration. It writes an empty
 * FAT table and initializes the root directory.
 *
 * @param fs_name Name of the new filesystem (file to create).
 * @param blocks_in_fat Number of blocks reserved for FAT.
 * @param block_size_config Block size selector (0–4 maps to 256–4096).
 * @return 0 on success, negative error code on failure.
 */
int mkfs(const char* fs_name, int blocks_in_fat, int block_size_config);

/**
 * @brief Mount a PennFAT filesystem.
 *
 * Opens the filesystem file, maps the FAT table into memory,
 * loads the root directory, and initializes file descriptors
 * for standard input, output, and error.
 *
 * @param fs_name Name of the filesystem file to mount.
 * @return 0 on success, negative error code on failure.
 */
int mount(const char* fs_name);

/**
 * @brief Unmount the currently mounted PennFAT filesystem.
 *
 * This function flushes and unmaps the FAT, writes the root directory
 * back to disk, and releases resources associated with the filesystem.
 *
 * @return 0 on success, negative error code on failure.
 */
int unmount();

/**
 * @brief Create new empty files.
 * 
 * @param argc Number of arguments.
 * @param argv Array of arguments (file names to create).
 * @return 0 on success, negative error code on failure.
 */
int ptouch(int argc, char* argv[]);

/**
 * @brief Rename or move a file from source to destination.
 * 
 * @param source Original file name.
 * @param dest New file name.
 * @return 0 on success, negative error code on failure.
 */
int mv(const char* source, const char* dest);

/**
 * @brief Remove a file from the filesystem.
 * 
 * @param filename Name of the file to remove.
 * @return 0 on success, negative error code on failure.
 */
int rm(const char* filename);

/**
 * @brief Concatenate and display/ write files.
 * 
 * @param argc Number of arguments.
 * @param argv Array of arguments.
 * @return 0 on success, negative error code on failure.
 */
int cat(int argc, char* argv[]);


/**
 * @brief Copy a file from host OS to PennFAT filesystem.
 * 
 * @param host_src Path to the host source file.
 * @param dest Destination file name in PennFAT.
 * @return 0 on success, negative error code on failure.
 */
int cp_host_to_pennfat(const char* host_src, const char* dest);

/**
 * @brief Copy a file from PennFAT to the host OS.
 * 
 * @param src Source file name in PennFAT.
 * @param host_dest Destination path on host.
 * @return 0 on success, negative error code on failure.
 */
int cp_pennfat_to_host(const char* src, const char* host_dest);

/**
 * @brief Copy a file within PennFAT from one file to another.
 * 
 * @param src Source file name.
 * @param dest Destination file name.
 * @return 0 on success, negative error code on failure.
 */
int cp_pennfat_to_pennfat(const char* src, const char* dest);

/**
 * @brief Dispatch and handle different types of copy operations.
 * 
 * @param argc Number of arguments.
 * @param argv Array of arguments.
 * @return 0 on success, negative error code on failure.
 */
int cp(int argc, char* argv[]);

/**
 * @brief Change the permission of a file.
 * 
 * @param filename Name of the file.
 * @param perm New permission to add.
 * @return 0 on success, negative error code on failure.
 */
int chmod(const char* filename, int perm);

/**
 * @brief List information about a file or all files in the directory.
 * 
 * @param filename Name of the file (if NULL, list all).
 * @return 0 on success, negative error code on failure.
 */
int ls(const char* filename);

#endif  // PENNFAT_H