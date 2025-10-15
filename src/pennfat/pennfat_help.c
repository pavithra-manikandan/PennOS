
#include "./pennfat_help.h"
#include "./kernel/kernel.h"
#include "./util/p_errno.h"
#include "./syscall/sys_call.h"


pennfat_state_t state = {0};


void k_print(const char* fmt, ...) {
    char buf[PRINT_BUFFER_SIZE];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        write(STDOUT_FILENO, buf, len);
    }
}

//Find the first free FAT entry
uint16_t find_free_fat_entry() {
  for (uint16_t i = 2; i < (state.fat_size / 2); i++) {
    if (state.fat[i] == FAT_ENTRY_FREE) {
      return i;
    }
  }
  return 0;  // No free blocks
}

// Locate a directory
dir_entry_t* find_dir_entry(const char* name) {
  dir_entry_t* entry = state.root_dir;
  int entries_per_block = state.block_size / sizeof(dir_entry_t);
  int total_entries = state.root_dir_blocks * entries_per_block;

  for (int i = 0; i < total_entries; i++, entry++) {
    if (entry->name[0] > DIR_ENTRY_DELETED && strcmp(entry->name, name) == 0) {
      return entry;
    }
  }
  return NULL;
}

//Sync directory entries
void sync_directory_entry(dir_entry_t* entry) {
  uint8_t* dir_ptr = (uint8_t*)state.root_dir;
  uint16_t block = 1;
  int written_blocks = 0;

  while (block != FAT_ENTRY_LAST && written_blocks < state.root_dir_blocks) {
    off_t offset = state.data_start + ((block - 1) * state.block_size);
    lseek(state.fs_fd, offset, SEEK_SET);
    write(state.fs_fd, dir_ptr, state.block_size);
    dir_ptr += state.block_size;
    block = state.fat[block];
    written_blocks++;
  }

  fsync(state.fs_fd);
}

//Touch command - Create new files
int ptouch(int argc, char* argv[]) {
  if (!state.is_mounted) {
    return FS_NOT_MOUNTED;
  }

  if (argc < 2) {
    return INVALID_MODE;
  }

  for (int i = 1; i < argc; i++) {
    const char* filename = argv[i];
    int fd = k_open(filename, F_WRITE);
    if (fd < 0) {
      k_print("Error creating file '%s': %d\n", filename, fd);
      continue;
    }
    k_close(fd);
  }
  sync_directory_entry(NULL);

  return 0;
}

//Mv command - Rename files
int mv(const char* source, const char* dest) {
  if (!state.is_mounted)
    return FS_NOT_MOUNTED;

  // Find source entry
  dir_entry_t* src = find_dir_entry(source);
  if (!src)
    return FILE_NOT_FOUND;
  // Check source permissions
  if (src->perm < 4 || src->perm > 7) {
    k_print("Read permission denied at source '%s'\n", source);
    return PERMISSION_DENIED;
  }
  // Check if destination exists
  dir_entry_t* dst = find_dir_entry(dest);
  if (dst) {
    int perm = dst->perm;
    if (perm != 2 && perm != 6 && perm != 7) {
      k_print("Write permission denied at destination %s\n", dest);
      return PERMISSION_DENIED;
    }
    rm(dest);
  }
  // Update name
  strncpy(src->name, dest, MAX_FILENAME_LEN - 1);
  src->name[MAX_FILENAME_LEN - 1] = '\0';
  src->mtime = time(NULL);
  sync_directory_entry(NULL);

  return 0;
}

//Rm - Remove files
int rm(const char* filename) {
  if (!state.is_mounted)
    return FS_NOT_MOUNTED;
  return k_unlink(filename);
}

//Cat functionality
int cat(int argc, char* argv[]) {
  if (!state.is_mounted)
    return FS_NOT_MOUNTED;

  char buf[1024];
  int write_mode = 0;
  int append_mode = 0;
  const char* output_filename = NULL;
  int retval = 0;

  // If no args or just argv[0] ("cat"), echo stdin
  if (argc <= 1 || argv == NULL) {
    while (1) {
      ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
      if (n < 0) {
        perror("read");
        return -1;
      } else if (n == 0) {
        break;  // EOF
      }
      write(STDOUT_FILENO, buf, n);
    }
    return 0;
  }

  // First pass: find output mode and output file
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-w") == 0) {
      write_mode = 1;
      if (i + 1 < argc) {
        output_filename = argv[++i];
      } else {
        return INVALID_MODE;
      }
    } else if (strcmp(argv[i], "-a") == 0) {
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
    out_fd = k_open(output_filename, append_mode ? F_APPEND : F_WRITE);
    if (out_fd < 0)
      return out_fd;
  }

  // Second pass: handle input files
  int input_found = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "-a") == 0) {
      i++;  // skip output file name after flag
      continue;
    }
    if (argv[i][0] == '-') continue;  // ignore unknown flags

    int in_fd = k_open(argv[i], F_READ);
    if (in_fd < 0) {
      k_print("%s: file doesn't exist\n", argv[i]);
      retval = 0;
      continue;
    }

    input_found = 1;
    ssize_t n;
    while ((n = k_read(in_fd, sizeof(buf), buf)) > 0) {
      if (out_fd >= 0) {
        if (k_write(out_fd, buf, n) != n) {
          retval = -1;
          break;
        }
      } else {
        write(STDOUT_FILENO, buf, n);
      }
    }
    k_close(in_fd);
  }

  // If no input files given, read from stdin
  if (!input_found && argc >2) {
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
      if (out_fd >= 0) {
        if (k_write(out_fd, buf, n) != n) {
          retval = -1;
          break;
        }
      } else {
        write(STDOUT_FILENO, buf, n);
      }
    }
  }

  if (out_fd >= 0)
    k_close(out_fd);

  return retval;
}

//Copy files from host to pennFAT
int cp_host_to_pennfat(const char* host_src, const char* dest) {
    int host_fd = open(host_src, O_RDONLY);
    if (host_fd < 0) {
        perror("Error opening host file");
        return -1;
    }

    int penn_fd = k_open(dest, F_WRITE);
    if (penn_fd < 0) {
        close(host_fd);
        return penn_fd;
    }

    char buf[1024];
    ssize_t n;
    while ((n = read(host_fd, buf, sizeof(buf))) > 0) {
        if (k_write(penn_fd, buf, n) != n) {
            close(host_fd);
            k_close(penn_fd);
            return -1;
        }
    }

    close(host_fd);
    return k_close(penn_fd);
}

//Copy files from pennFAT to host
int cp_pennfat_to_host(const char* src, const char* host_dest) {
    int host_fd = open(host_dest, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (host_fd < 0) {
        perror("Error creating host file");
        return -1;
    }

    int penn_fd = k_open(src, F_READ);
    if (penn_fd < 0) {
        close(host_fd);
        return penn_fd;
    }

    char buf[1024];
    ssize_t n;
    while ((n = k_read(penn_fd, sizeof(buf), buf)) > 0) {
        if (write(host_fd, buf, n) != n) {
            close(host_fd);
            k_close(penn_fd);
            return -1;
        }
    }

    close(host_fd);
    return k_close(penn_fd);
}

//Copy files to and from pennFAT
int cp_pennfat_to_pennfat(const char* src, const char* dest) {
    dir_entry_t* src_entry = find_dir_entry(src);
    if (!src_entry || src_entry->name[0] == DIR_ENTRY_DELETED) {
        k_print("Error: Source file '%s' not found\n", src);
        return FILE_NOT_FOUND;
    }

    int src_fd = k_open(src, F_READ);
    if (src_fd < 0)
        return src_fd;

    int dest_fd = k_open(dest, F_WRITE);
    if (dest_fd < 0) {
        k_close(src_fd);
        return dest_fd;
    }

    char buf[1024];
    ssize_t n;
    while ((n = k_read(src_fd, sizeof(buf), buf)) > 0) {
        if (k_write(dest_fd, buf, n) != n) {
            k_close(src_fd);
            k_close(dest_fd);
            return -1;
        }
    }

    k_close(src_fd);
    return k_close(dest_fd);
}

//Cp command - To copy files
int cp(int argc, char* argv[]) {
    if (!state.is_mounted)
        return FS_NOT_MOUNTED;

    int host_mode = 0;
    const char* src = NULL;
    const char* dest = NULL;

    if (argc >= 4 && strcmp(argv[1], "-h") == 0) {
        host_mode = 1;
        src = argv[2];
        dest = argv[3];
    } else if (argc >= 4 && strcmp(argv[2], "-h") == 0) {
        host_mode = 2;
        src = argv[1];
        dest = argv[3];
    } else if (argc == 3) {
        src = argv[1];
        dest = argv[2];
    } else {
        k_print(
            "Usage:\n"
            " cp <src> <dest>\n"
            " cp -h <host_src> <dest>\n"
            " cp <src> -h <host_dest>\n");
        return INVALID_MODE;
    }

    if (host_mode == 1)
        return cp_host_to_pennfat(src, dest);
    else if (host_mode == 2)
        return cp_pennfat_to_host(src, dest);
    else
        return cp_pennfat_to_pennfat(src, dest);
}

//Chmod command - Change permissions
int chmod(const char* filename, int perm) {
  if (!state.is_mounted)
    return FS_NOT_MOUNTED;

  dir_entry_t* entry = find_dir_entry(filename);
  if (!entry)
    return FILE_NOT_FOUND;
  int new_perm = entry->perm + perm;
  if (new_perm < 0 || new_perm > 7)
    return INVALID_MODE;
  entry->perm = new_perm;

  entry->mtime = time(NULL);
  sync_directory_entry(NULL);
  return 0;
}

//Ls command - List directory entries
int ls(const char* filename) {
  return k_ls(filename);
}