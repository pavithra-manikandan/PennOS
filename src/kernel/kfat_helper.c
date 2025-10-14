#include "./kfat_helper.h"
#include "./syscall/sys_call.h"
#include "./util/p_errno.h"


// K_OPEN WITH HELPERS:
int find_open_fd(const char* fname, int mode) {
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    dir_entry_t* entry = state.open_files[i].entry;
    if (entry && strcmp(entry->name, fname) == 0) {
      uint8_t perm = entry->perm;
      if ((mode == F_WRITE || mode == F_APPEND) &&
          (perm != PERM_READ_WRITE && perm != PERM_WRITE && perm != PERM_ALL)) {
        return PERMISSION_DENIED;
      }
      if (perm == PERM_NONE) return PERMISSION_DENIED;
      state.open_files[i].ref_count++;
      return i;
    }
  }
  return -1;  // Not found
}

dir_entry_t* find_or_create_entry(const char* fname, int mode) {
  dir_entry_t* entry = find_dir_entry(fname);
  if (entry) return entry;

  if (mode != F_WRITE) return NULL;  // Only create on WRITE

  // Find free directory slot
  entry = state.root_dir;
  while (entry->name[0] != DIR_ENTRY_END) {
    if (entry->name[0] == DIR_ENTRY_DELETED || entry->name[0] < DIR_ENTRY_IN_USE)
      break;
    entry++;
  }

  int dir_entries_per_block = state.block_size / sizeof(dir_entry_t);
  int total_entries = state.root_dir_blocks * dir_entries_per_block;
  int valid_entries = 0;
  for (int i = 0; i < total_entries; i++) {
    if (state.root_dir[i].name[0] > DIR_ENTRY_DELETED) valid_entries++;
  }

  if (valid_entries >= total_entries - 1) {  // Need expansion
    uint16_t new_block = find_free_fat_entry();
    if (!new_block) return NULL;
    uint16_t last = 1;
    while (state.fat[last] != FAT_ENTRY_LAST) last = state.fat[last];
    state.fat[last] = new_block;
    state.fat[new_block] = FAT_ENTRY_LAST;

    void* new_root = realloc(state.root_dir, (state.root_dir_blocks + 1) * state.block_size);
    if (!new_root) return NULL;
    state.root_dir = new_root;
    memset((uint8_t*)state.root_dir + state.root_dir_blocks * state.block_size, 0, state.block_size);
    state.root_dir_blocks++;
    ((dir_entry_t*)((uint8_t*)state.root_dir + (state.root_dir_blocks - 1) * state.block_size))->name[0] = DIR_ENTRY_END;

    // Re-scan for new free entry
    entry = state.root_dir;
    while (entry->name[0] != DIR_ENTRY_END) {
      if (entry->name[0] == DIR_ENTRY_DELETED || entry->name[0] < DIR_ENTRY_IN_USE)
        break;
      entry++;
    }
  }

  memset(entry, 0, sizeof(dir_entry_t));
  strncpy(entry->name, fname, MAX_FILENAME_LEN - 1);
  entry->name[MAX_FILENAME_LEN - 1] = '\0';
  entry->size = 0;
  entry->first_block = find_free_fat_entry();
  if (!entry->first_block) return NULL;
  state.fat[entry->first_block] = FAT_ENTRY_LAST;
  entry->type = FT_REGULAR;
  entry->perm = PERM_READ_WRITE;
  entry->mtime = time(NULL);

  return entry;
}

int allocate_fd(dir_entry_t* entry, int mode) {
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (!state.open_files[i].entry) {
      state.open_files[i] = (file_descriptor_t){
          .fd = i,
          .entry = entry,
          .current_block = entry->first_block,
          .offset = (mode == F_APPEND) ? entry->size : 0,
          .mode = mode,
          .ref_count = 1};

      if (mode == F_WRITE) {
        entry->size = 0;
        uint16_t curr = state.fat[entry->first_block];
        state.fat[entry->first_block] = FAT_ENTRY_LAST;
        while (curr != FAT_ENTRY_LAST) {
          uint16_t next = state.fat[curr];
          state.fat[curr] = FAT_ENTRY_FREE;
          curr = next;
        }
      }
      return i;
    }
  }
  P_ERRNO = FD_INVALID;
  return -1;
}


int k_open(const char* fname, int mode) {
  // Validate mounted FS and mode
  if (!state.is_mounted || !state.fat || !state.root_dir) { P_ERRNO = FS_NOT_MOUNTED; return -1; }
  if (mode != F_READ && mode != F_WRITE && mode != F_APPEND) { P_ERRNO = INVALID_MODE; return -1; }

  // Check if already open
  int fd = find_open_fd(fname, mode);
  if (fd >= 0) return fd;  // Already open

  // Find or create file entry
  dir_entry_t* entry = find_or_create_entry(fname, mode);
  if (!entry) { P_ERRNO = FILE_NOT_FOUND; return -1; }

  // Allocate FD
  return allocate_fd(entry, mode);
}


int k_read(int fd, int n, char* buf) {
  // Validate FD
  if (fd < 0 || fd >= MAX_OPEN_FILES || !state.open_files[fd].entry) {
    P_ERRNO = FD_INVALID;
    return -1;
  }

  file_descriptor_t* file = &state.open_files[fd];
  dir_entry_t* entry = file->entry;

  // Check permissions
  if (!(entry->perm & PERM_READ)) {
    P_ERRNO = PERMISSION_DENIED;
    return -1;
  }

  // Read from stdin (special handling)
  if (fd == 0 && strcmp(entry->name, "stdin") == 0) {
    size_t input_len = 0;
    char* local_buf = (char*)malloc(4096 * sizeof(char));
    ssize_t bytes_read = getline(&local_buf, &input_len, stdin);

    if (bytes_read == -1) {
      if (feof(stdin)) {
        clearerr(stdin);  // Reset EOF so future reads work
        return 0;         // Treat as EOF
      }
      P_ERRNO = FD_INVALID;
      return -1;  // Error
    }
    local_buf[strcspn(local_buf, "\n")] = '\0';  // Remove newline
    strncpy(buf, local_buf, bytes_read + 1);
    free(local_buf);
    return (int)bytes_read;
  }

  // Regular file read
  if (entry->size == 0 || file->current_block == FAT_ENTRY_LAST) {
    return 0;
  }

  int bytes_read = 0;
  uint16_t current_block = file->current_block;
  uint32_t offset_in_block = file->offset % state.block_size;

  while (bytes_read < n && current_block != FAT_ENTRY_LAST) {
    int bytes_remaining_in_block = state.block_size - offset_in_block;
    int bytes_to_read = MIN(MIN(bytes_remaining_in_block, n - bytes_read),
                            entry->size - file->offset);

    if (bytes_to_read <= 0)
      break;

    lseek(state.fs_fd,
          state.data_start + (current_block - 1) * state.block_size +
              offset_in_block,
          SEEK_SET);
    int chunk = read(state.fs_fd, buf + bytes_read, bytes_to_read);
    if (chunk < 0) {
      P_ERRNO = FD_INVALID;
      return -1;
    }

    bytes_read += chunk;
    file->offset += chunk;
    offset_in_block = 0;

    if (file->offset % state.block_size == 0) {
      current_block = state.fat[current_block];
      file->current_block = current_block;
    }
  }

  return bytes_read;
}

int k_cat(int argc, char* argv[]) {
  if (!state.is_mounted)
    return FS_NOT_MOUNTED;

  char buf[1024];
  int write_mode = 0;
  int append_mode = 0;
  const char* output_filename = NULL;
  int retval = 0;
  // First pass: find output mode and output file
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], ">") == 0) {
      write_mode = 1;
      if (i + 1 < argc) {
        output_filename = argv[++i];
        printf("k_cat output_filename = %s\n", output_filename);
      } else {
        return INVALID_MODE;
      }
    } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], ">>") == 0) {
      write_mode = 1;
      append_mode = 1;
      if (i + 1 < argc) {
        output_filename = argv[++i];
      } else {
        return INVALID_MODE;
      }
    }
  }

  // Open output file if needed
  int out_fd = -1;
  if (write_mode && output_filename) {
    out_fd = s_open(output_filename, append_mode ? F_APPEND : F_WRITE);
    if (out_fd < 0)
      return out_fd;
  }

  // Second pass: handle input files
  int input_found = 0;
  for (int i = 1; i < argc; i++) {
    // skip flags and output filename
    if ((strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "-w") == 0 ||
         strcmp(argv[i], ">>") == 0 || strcmp(argv[i], ">") == 0)) {
      continue;
    }
    if (argv[i][0] == '-')
      continue;
    if (output_filename) {
      if (strcmp(argv[i], output_filename) == 0 && append_mode == 1) {
        printf("Cat may not read and append to the same file");
        return INVALID_MODE;
      }
    }

    int in_fd = s_open(argv[i], F_READ);
    if (in_fd < 0) {
      retval = in_fd;
      continue;
    }

    input_found = 1;
    int n;
    while ((n = s_read(in_fd, sizeof(buf), buf)) > 0) {
      if (out_fd >= 0) {
        if (s_write(out_fd, n, buf) != n) {
          retval = -1;
          break;
        }
      } else {
        s_write(STDOUT_FILENO, n, buf);
      }
    }

    s_close(in_fd);
  }

  // If no input file: read from stdin and write to output or stdout
  if (!input_found && argc == 1) {
    int n;
    proc_fd_ent* file_table = get_file_descriptors();
    if (file_table[STDIN_FILENO].global_fd ==
            file_table[STDOUT_FILENO].global_fd &&
        file_table[STDOUT_FILENO].mode == F_APPEND) {
      printf("Cat may not read and append to the same file");
      return INVALID_MODE;
    }
    while ((n = s_read(STDIN_FILENO, sizeof(buf), buf)) > 0) {
      if (out_fd >= 0) {
        if (s_write(out_fd, n, buf) != n) {
          retval = -1;
          break;
        }
      } else {
        if(s_write(STDOUT_FILENO, n, buf) == -1) {
          u_perror("s_write: Failed");
        }
      }
    }
    if (file_table[STDIN_FILENO].global_fd != 0) {
      s_close(STDIN_FILENO);
      file_table[STDIN_FILENO] = (proc_fd_ent){
          .proc_fd = 0, .mode = F_READ, .offset = 0, .global_fd = 0};
    }
    if (file_table[STDOUT_FILENO].global_fd != 1) {
      s_close(STDOUT_FILENO);
      file_table[STDOUT_FILENO] = (proc_fd_ent){
          .proc_fd = 1, .mode = F_WRITE, .offset = 0, .global_fd = 1};
    }
  }

  if (out_fd >= 0)
    s_close(out_fd);
  sync_directory_entry(NULL);
  return retval;
}

int k_write(int fd, const char* buf, int n) {
  // Validate FD and permissions
  if (fd < 0 || fd >= MAX_OPEN_FILES || !state.open_files[fd].entry) {
    P_ERRNO = FD_INVALID;
    return -1;
  }

  file_descriptor_t* file = &state.open_files[fd];
  dir_entry_t* entry = file->entry;

  if (!(entry->perm & PERM_WRITE)) {
    P_ERRNO = PERMISSION_DENIED;
    return -1;
  }
  if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
    int bytes_written = write(fd, buf, n);
    return bytes_written;
  }
  // Handle append mode
  if (file->mode == F_APPEND) {
    file->offset = entry->size;
    if (entry->first_block != FAT_ENTRY_LAST) {
      uint16_t block = entry->first_block;
      while (state.fat[block] != FAT_ENTRY_LAST) {
        block = state.fat[block];
      }
      file->current_block = block;
    }
  }

  int bytes_written = 0;
  uint16_t current_block = file->current_block;
  uint32_t offset_in_block = file->offset % state.block_size;

  // Write loop
  while (bytes_written < n) {
    // Allocate new block if needed
    if (current_block == FAT_ENTRY_LAST ||
        (offset_in_block == state.block_size && bytes_written < n)) {
      uint16_t new_block = find_free_fat_entry();
      if (new_block == 0) {
        return DISK_FULL;
      }

      if (current_block == FAT_ENTRY_LAST) {
        entry->first_block = new_block;
      } else {
        state.fat[current_block] = new_block;
      }

      state.fat[new_block] = FAT_ENTRY_LAST;
      current_block = new_block;
      offset_in_block = 0;
      file->current_block = current_block;
    }

    // Calculate write size
    int remaining_in_block = state.block_size - offset_in_block;
    int bytes_to_write = MIN(remaining_in_block, n - bytes_written);

    // Seek and write
    off_t seek_pos = state.data_start + (current_block - 1) * state.block_size +
                     offset_in_block;
    lseek(state.fs_fd, seek_pos, SEEK_SET);

    int chunk = write(state.fs_fd, buf + bytes_written, bytes_to_write);
    if (chunk < 0) {
      P_ERRNO = FD_INVALID;
      return -1;
    }

    // Update state
    bytes_written += chunk;
    offset_in_block += chunk;
    file->offset += chunk;

    // Always update size to current write head position
    entry->size = file->offset;
    sync_directory_entry(entry);
  }

  // Sync metadata

  entry->mtime = time(NULL);
  msync(state.fat, state.fat_size, MS_SYNC);
  fsync(state.fs_fd);  // Ensure data hits disk

  return bytes_written;
}

int k_unlink(const char* fname) {
  if (!state.is_mounted)
    return FS_NOT_MOUNTED;

  // Find the file
  dir_entry_t* entry = find_dir_entry(fname);
  if (!entry) {
    return FILE_NOT_FOUND;
  }

  // Check if file is open
  // for (int i = 0; i < MAX_OPEN_FILES; i++) {
  //   if (state.open_files[i].entry == entry) {
  //     return FILE_IN_USE;
  //   }
  // }
  bool is_open = false;
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (state.open_files[i].entry == entry) {
      is_open = true;
      break;
    }
  }

  if (is_open) {
    // Mark as deleted but in-use
    entry->name[0] = 2;  // Special "deleted but in-use" marker
    return 0;
  }

  // Regular deletion
  entry->name[0] = DIR_ENTRY_DELETED;  // 1
  memset(entry->name + 1, 0, MAX_FILENAME_LEN - 1);

  // Free FAT blocks with safety check
  uint16_t block = entry->first_block;
  int max_blocks = (state.fat_size / 2);
  int blocks_freed = 0;

  while (block != FAT_ENTRY_LAST && block != FAT_ENTRY_FREE &&
         blocks_freed < max_blocks) {
    uint16_t next = state.fat[block];
    state.fat[block] = FAT_ENTRY_FREE;
    block = next;
    blocks_freed++;
  }

  // Mark as deleted and clear name
  memset(entry->name, 0, MAX_FILENAME_LEN);
  entry->name[0] = DIR_ENTRY_DELETED;

  // Write updated directory block to disk (block 1)
  off_t dir_pos = state.block_size;  // Block 0 is FAT, Block 1 is root dir
  lseek(state.fs_fd, dir_pos, SEEK_SET);
  write(state.fs_fd, state.root_dir, state.block_size);

  // Force sync all changes
  msync(state.fat, state.fat_size, MS_SYNC);
  fsync(state.fs_fd);
  sync_directory_entry(NULL);

  return 0;
}

int k_lseek(int fd, int offset, int whence) {
  file_descriptor_t* file;
  dir_entry_t* entry;
  if (fd < 0 || fd >= MAX_OPEN_FILES || !state.open_files[fd].entry) {
    return FD_INVALID;
  }

  file = &state.open_files[fd];
  entry = file->entry;

  uint32_t new_offset;
  switch (whence) {
    case F_SEEK_SET:
      new_offset = offset;
      break;
    case F_SEEK_CUR:
      new_offset = file->offset + offset;
      break;
    case F_SEEK_END:
      new_offset = entry->size + offset;
      break;
    default:
      return INVALID_WHENCE;  // Define this error
  }

  // Validate new offset
  if (new_offset > entry->size) {
    // For writes, this is allowed (extends file)
    // For reads, clamp to EOF
    if (!(file->mode & (F_WRITE | F_APPEND))) {
      new_offset = entry->size;
    }
  }

  // Update block pointer if crossing block boundaries
  if (new_offset / state.block_size != file->offset / state.block_size) {
    uint16_t block = entry->first_block;
    uint32_t target_block = new_offset / state.block_size;

    for (uint32_t i = 0; i < target_block && block != FAT_ENTRY_LAST; i++) {
      block = state.fat[block];
    }
    file->current_block = block;
  }

  file->offset = new_offset;
  return new_offset;
}

int k_perm(const char* fname) {
  dir_entry_t* entry = find_dir_entry(fname);
  if (entry == NULL) {
    return FILE_NOT_FOUND;
  } else {
    return entry->perm;
  }
}

int k_close(int fd) {
  // Validate FD
  if (fd < 0 || fd >= MAX_OPEN_FILES || !state.open_files[fd].entry) {
    P_ERRNO = FD_INVALID;
    return -1;
  }

  // Clear the FD entry if ref_count is 0
  state.open_files[fd].ref_count--;
  if (state.open_files[fd].ref_count <= 0) {
    memset(&state.open_files[fd], 0, sizeof(file_descriptor_t));
    state.open_files[fd].fd = -1;
  }

  return 0;  // Success
}

int k_ls(const char* filename) {
  if (!state.is_mounted)
    return FS_NOT_MOUNTED;

  if (filename) {
    // Single file listing
    dir_entry_t* entry = find_dir_entry(filename);
    if (!entry)
      return FILE_NOT_FOUND;

    k_print("%6u %c%c%c %8u %s\n", entry->first_block,
           (entry->perm & PERM_READ) ? 'r' : '-',
           (entry->perm & PERM_WRITE) ? 'w' : '-',
           (entry->perm & PERM_EXEC) == PERM_EXEC ? 'x' : '-', entry->size,
           entry->name);

  } else {
    dir_entry_t* entry = state.root_dir;
    int entries_per_block = state.block_size / sizeof(dir_entry_t);
    int total_entries = state.root_dir_blocks * entries_per_block;

    for (int i = 0; i < total_entries; i++, entry++) {
      if (entry->name[0] == DIR_ENTRY_END)
        break;
      if (entry->name[0] > DIR_ENTRY_DELETED) {


        printf("%6u %c%c%c %8u %.24s %s\n", entry->first_block,
               (entry->perm & PERM_READ) ? 'r' : '-',
               (entry->perm & PERM_WRITE) ? 'w' : '-',
               (entry->perm & PERM_EXEC) == PERM_EXEC ? 'x' : '-', entry->size,
               ctime(&entry->mtime), entry->name);
      }
      
    }
  }
  return 0;
}

int k_file_size(const char* filename) {
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    dir_entry_t* entry = state.open_files[i].entry;
    if (entry != NULL && strcmp(entry->name, filename) == 0) {
      return entry->size;
    }
  }
  dir_entry_t* entry = find_dir_entry(filename);
  if (entry != NULL && strcmp(entry->name, filename) == 0) {
    return entry->size;
  }
  return FILE_NOT_FOUND;
}