#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "log.h"

#define LOG_BUFFER_SIZE 256
#define LOG_DIR "./log/" // Directory for log files
#define DEFAULT_LOG_FILE "./log/log"

static int log_fd = -1;
static unsigned long clock_ticks = 0; // Global tick counter

// Ensure the log directory exists
void ensure_log_directory() {
    struct stat st;
    if (stat(LOG_DIR, &st) == -1) {
        if (mkdir(LOG_DIR, 0755) == -1) {
            perror("Failed to create log directory");
            exit(EXIT_FAILURE);
        }
    }
}

// Initialize logging system
void log_init(const char* filename) {
    ensure_log_directory();

    char filepath[LOG_BUFFER_SIZE];
    if (filename) {
        snprintf(filepath, LOG_BUFFER_SIZE, "%s%s", LOG_DIR, filename);
    } else {
        snprintf(filepath, LOG_BUFFER_SIZE, "%s", DEFAULT_LOG_FILE);
    }

    log_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd == -1) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
}

// Log an event with variable arguments
void log_event(const char* operation, const char* fmt, ...) {
    if (log_fd == -1) return;

    char buffer[LOG_BUFFER_SIZE];
    int offset = snprintf(buffer, LOG_BUFFER_SIZE, "[%lu]\t%s", clock_ticks, operation);

    va_list args;
    va_start(args, fmt);

    offset += vsnprintf(buffer + offset, LOG_BUFFER_SIZE - offset, fmt, args);

    va_end(args);

    snprintf(buffer + offset, LOG_BUFFER_SIZE - offset, "\n");
    write(log_fd, buffer, strlen(buffer));
}

// Update clock ticks (called in scheduler tick)
void log_tick() {
    clock_ticks++;
}

// Close log file
void log_close() {
    if (log_fd != -1) {
        close(log_fd);
    }
}
