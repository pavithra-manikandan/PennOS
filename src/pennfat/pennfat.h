#ifndef PENNFATT_H
#define PENNFATT_H
#include "./pennfat_help.h"

// Global state
// pennfat_state_t state = {0};

#define PRINT_BUFFER_SIZE 256



int mkfs(const char* fs_name, int blocks_in_fat, int block_size_config);
// Mount PennFAT
int pmount(const char* fs_name);

int punmount();

#endif