#include "./pennfat.h"
#include "./util/p_errno.h"


int mkfs(const char* fs_name, int blocks_in_fat, int block_size_config) {
    if (state.is_mounted)
        return FS_NOT_MOUNTED;
    if (blocks_in_fat < 1 || blocks_in_fat > 32)
        return INVALID_FAT_CONFIG;
    if (block_size_config < 0 || block_size_config > 4)
        return INVALID_FAT_CONFIG;

    const int block_sizes[] = {256, 512, 1024, 2048, 4096};
    int block_size = block_sizes[block_size_config];

    uint32_t fat_entries = (blocks_in_fat * block_size) / 2;
    uint32_t data_blocks = fat_entries - 1;
    uint32_t total_blocks = blocks_in_fat + data_blocks;
    uint32_t total_size = total_blocks * block_size;

    // Special case for maximum size
    if (blocks_in_fat == 32 && block_size_config == 4) {
        total_size -= 4096;
    }

    // Create and open the filesystem file
    int fd = open(fs_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return -1;

    // Allocate and initialize FAT
    uint16_t* fat = malloc(fat_entries * sizeof(uint16_t));
    if (!fat) {
        close(fd);
        return -1;
    }

    // Explicit FAT initialization
    fat[0] = (uint16_t)((blocks_in_fat << 8) | block_size_config); // Header
    fat[1] = FAT_ENTRY_LAST; // Root directory marker
    
    // Explicitly mark all other blocks as free
    for (uint32_t i = 2; i < fat_entries; i++) {
        fat[i] = FAT_ENTRY_FREE;
    }

    // Write FAT to disk
    ssize_t written = write(fd, fat, fat_entries * sizeof(uint16_t));
    if (written != fat_entries * sizeof(uint16_t)) {
        free(fat);
        close(fd);
        return -1;
    }

    // Pad FAT region if needed (using dynamic zero buffer)
    uint32_t fat_bytes_total = blocks_in_fat * block_size;
    uint32_t fat_bytes_written = fat_entries * sizeof(uint16_t);
    
    if (fat_bytes_written < fat_bytes_total) {
        uint8_t* zero_block = calloc(1, block_size); // Block-sized zero buffer
        if (!zero_block) {
            free(fat);
            close(fd);
            return -1;
        }
        write(fd, zero_block, fat_bytes_total - fat_bytes_written);
        free(zero_block);
    }

    // Initialize root directory block
    uint8_t* zero_block = calloc(1, block_size);
    if (!zero_block) {
        free(fat);
        close(fd);
        return -1;
    }
    
    // Set directory end marker
    dir_entry_t* dir = (dir_entry_t*)zero_block;
    dir[0].name[0] = DIR_ENTRY_END;
    
    written = write(fd, zero_block, block_size);
    free(zero_block);
    if (written != block_size) {
        free(fat);
        close(fd);
        return -1;
    }

    // Set final size
    if (ftruncate(fd, total_size) < 0) {
        free(fat);
        close(fd);
        return -1;
    }

    free(fat);
    close(fd);
    return 0;
}

// Mount PennFAT
int pmount(const char* fs_name) {
  if (state.is_mounted)
    return FS_NOT_MOUNTED;

  state.fs_fd = open(fs_name, O_RDWR);
  if (state.fs_fd < 0)
    return -1;

  // Read FAT[0] to get config
  uint16_t fat_entry_zero;
  read(state.fs_fd, &fat_entry_zero, 2);

  state.block_size = 256 << (fat_entry_zero & 0xFF);  // LSB = block size config
  state.fat_blocks = fat_entry_zero >> 8;             // MSB = blocks in FAT
  state.fat_size = state.block_size * state.fat_blocks;

  // Memory-map FAT
  state.fat = mmap(NULL, state.fat_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   state.fs_fd, 0);
  if (state.fat == MAP_FAILED)
    return -1;

  // Initialize root directory (starts at Block 1)
  state.data_start = state.fat_size;
    uint16_t root_block = 1;
    uint32_t max_dir_blocks = 64; // Choose based on memory or FAT size
    state.root_dir = malloc(max_dir_blocks * state.block_size);
    if (!state.root_dir)
        return -1;

    uint8_t* dir_ptr = (uint8_t*)state.root_dir;
    uint32_t blocks_read = 0;

    while (root_block != FAT_ENTRY_LAST && blocks_read < max_dir_blocks) {
        off_t offset = state.data_start + ((root_block-1) * state.block_size);
        lseek(state.fs_fd, offset, SEEK_SET);
        read(state.fs_fd, dir_ptr, state.block_size);
        dir_ptr += state.block_size;
        root_block = state.fat[root_block];
        blocks_read++;
    }

    // Store how many blocks we loaded
    state.root_dir_blocks = blocks_read;
    
    // Initialize stdin, stdout, stderr
    dir_entry_t *stdin = (dir_entry_t*)malloc(sizeof(dir_entry_t));
    strncpy(stdin->name, "stdin", MAX_FILENAME_LEN - 1);
    stdin->name[MAX_FILENAME_LEN - 1] = '\0';
    stdin->perm = PERM_READ;
    file_descriptor_t stdin_fd = {0};
    stdin_fd.fd = 0;
    stdin_fd.current_block = 0;
    stdin_fd.offset = 0;
    stdin_fd.mode = F_READ;
    stdin_fd.ref_count = 1;
    stdin_fd.entry = stdin;
  
    dir_entry_t *stdout = (dir_entry_t*)malloc(sizeof(dir_entry_t));
    strncpy(stdout->name, "stdout", MAX_FILENAME_LEN - 1);
    stdout->name[MAX_FILENAME_LEN - 1] = '\0';
    stdout->perm = PERM_WRITE;
    file_descriptor_t stdout_fd = {0};
    stdout_fd.fd = 1;
    stdout_fd.current_block = 0;
    stdout_fd.offset = 0;
    stdout_fd.mode = F_WRITE;
    stdout_fd.ref_count = 1;
    stdout_fd.entry = stdout;
  
    dir_entry_t *stderr = (dir_entry_t*)malloc(sizeof(dir_entry_t));
    strncpy(stderr->name, "stderr", MAX_FILENAME_LEN - 1);
    stderr->name[MAX_FILENAME_LEN - 1] = '\0';
    stderr->perm = PERM_WRITE;
    file_descriptor_t stderr_fd = {0};
    stderr_fd.fd = 2;
    stderr_fd.current_block = 0;
    stderr_fd.offset = 0;
    stderr_fd.mode = F_WRITE;
    stderr_fd.ref_count = 1;
    stderr_fd.entry = stderr;

    state.open_files[0] = stdin_fd;
    state.open_files[1] = stdout_fd;
    state.open_files[2] = stderr_fd;
    state.is_mounted = 1;

  for (int i = 3; i < MAX_OPEN_FILES; i++) {
    state.open_files[i].fd = -1;
   }
  return 0;
}

//Unmount PennFAT
int punmount() {
    //Check mount status
    if (!state.is_mounted) {
        return FS_NOT_MOUNTED;
    }

    //Check for open files (critical for tests)
    for (int i = MAX_OPEN_FILES; i >= 0; i--) {
        if (i >= 0 && i <= 2) {
            free(state.open_files[i].entry);
        } else if (state.open_files[i].entry != NULL) {
            return FILE_IN_USE;
        }
    }

    //Write root directory to disk (REQUIRED to prevent corruption)
    if (state.root_dir && state.fs_fd >= 0) {
        uint8_t* dir_ptr = (uint8_t*)state.root_dir;
        uint16_t block = 1;
        int written_blocks = 0;

        while (block != FAT_ENTRY_LAST && written_blocks < state.root_dir_blocks) {
            off_t offset = state.data_start + ((block-1) * state.block_size);
            lseek(state.fs_fd, offset, SEEK_SET);
            write(state.fs_fd, dir_ptr, state.block_size);
            dir_ptr += state.block_size;
            block = state.fat[block];
            written_blocks++;
        }

        fsync(state.fs_fd);  // Ensure metadata flush
    }
    //Free root_dir
    free(state.root_dir);
    //Sync FAT
    if (state.fat && msync(state.fat, state.fat_size, MS_SYNC) < 0) {
        return FS_IO_ERROR;
    }

    //Unmap FAT
    if (state.fat != NULL && state.fat != MAP_FAILED) {
        munmap(state.fat, state.fat_size);
        state.fat = NULL;
    }

    //Close file descriptor
    if (state.fs_fd >= 0) {
        close(state.fs_fd);
        state.fs_fd = -1;
    }

    //Reset state (minimal fields)
    state.is_mounted = 0;
    state.block_size = 0;
    state.fat_blocks = 0;
    state.fat_size = 0;
    state.data_start = 0;

    return 0;  // FS_SUCCESS
}

#ifdef STANDALONE_FAT
int main(int argc, char* argv[]) {
    memset(&state, 0, sizeof(pennfat_state_t));
    char input[256];

    while (1) {
        k_print("pennfat> ");
        if (!fgets(input, sizeof(input), stdin)) {
            break;  // Exit on EOF (Ctrl+D)
        }

        // Remove trailing newline
        input[strcspn(input, "\n")] = '\0';

        // Skip empty lines
        if (input[0] == '\0')
            continue;

        // Tokenize entire input into args[]
        char* args[16] = {0};
        int arg_count = 0;

        char* token = strtok(input, " ");
        while (token && arg_count < 15) {
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }

        if (arg_count == 0)
            continue;

        char* cmd = args[0];

        // ------------------ Command handlers ------------------

        if (strcmp(cmd, "mkfs") == 0) {
            if (arg_count == 4) {
                int blocks = atoi(args[2]);
                int size = atoi(args[3]);
                int ret = mkfs(args[1], blocks, size);
                if (ret != 0) // FS_NOT_MOUNTED; INVALID_FAT_CONFIG
                    k_print("Error %d\n", ret);
            } else {
                k_print("Usage: mkfs <fs_name> <blocks_in_fat> <block_size_config>\n");
            }
        } else if (strcmp(cmd, "mount") == 0) {
            if (arg_count == 2) {
                int ret = pmount(args[1]);
                if (ret != 0) // FD_INVALID; 
                    k_print("Error %d\n", ret);
            } else {
                k_print("Usage: mount <fs_name>\n");
            }
        } else if (strcmp(cmd, "unmount") == 0) {
            int ret = punmount();
            if (ret != 0) // FS_NOT_MOUNTED; FILE_IN_USE
                k_print("Error %d\n", ret);
        } else if (strcmp(cmd, "touch") == 0) {
            if (arg_count == 2) {
                int ret = ptouch(arg_count, args);
                if (ret != 0) // FS_NOT_MOUNTED; INVALID_MODE
                    k_print("Error %d\n", ret);
            } else if (arg_count > 2){
                int ret = ptouch(arg_count, args);
                if (ret != 0)
            k_print("Error %d\n", ret);
            } else {
                k_print("Usage: touch <filename>\n");
            }
        } else if (strcmp(cmd, "mv") == 0) {
            if (arg_count == 3) {
                int ret = mv(args[1], args[2]);
                if (ret != 0)
                    k_print("Error %d\n", ret);
            } else {
                k_print("Usage: mv <source> <dest>\n");
            }
        } else if (strcmp(cmd, "rm") == 0) {
            if (arg_count == 2) {
                int ret = rm(args[1]);
                if (ret != 0)
                    k_print("Error %d\n", ret);
            } else {
                k_print("Usage: rm <filename>\n");
            }
        } else if (strcmp(cmd, "chmod") == 0) {
            if (arg_count == 3) {
                int perm = atoi(args[2]);
                int ret = chmod(args[1], perm);
                if (ret != 0)
                    k_print("Error %d\n", ret);
            } else {
                k_print("Usage: chmod <filename> <perm>\n");
            }
        } else if (strcmp(cmd, "ls") == 0) {
            const char* fname = (arg_count == 2) ? args[1] : NULL;
            int ret = ls(fname);
            if (ret != 0)
                k_print("Error %d\n", ret);

        } else if (strcmp(cmd, "cp") == 0) {
            int ret = cp(arg_count, args);
            if (ret != 0)
                k_print("Error %d\n", ret);

        } else if (strcmp(cmd, "cat") == 0) {
            int ret = cat(arg_count, args);
            if (ret != 0)
                k_print("Error %d\n", ret);
        } else {
            k_print("Unknown command.\n");
        }
    }

    if (state.is_mounted)
        punmount();
    return 0;
}
#endif

