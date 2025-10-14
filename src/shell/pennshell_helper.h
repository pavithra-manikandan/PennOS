#ifndef PENNSHELL_HELPER_H
#define PENNSHELL_HELPER_H

#include <termios.h>
#include "./kernel/kernel.h"
#include "./pcb.h"
#define HISTORY_LIMIT 20

extern pid_t current_foreground_pid;

/**
 * @brief Handles SIGINT (Ctrl+C) by sending a termination signal to the foreground process.
 * 
 * @param sig Signal number (should be SIGINT).
 */
void sigint_handler(int sig);

/**
 * @brief Handles SIGTSTP (Ctrl+Z) by sending a stop signal to the foreground process.
 * 
 * @param sig Signal number (should be SIGTSTP).
 */
void sigstop_handler(int sig);

/**
 * @brief Sets up SIGINT and SIGTSTP handlers for foreground process control.
 */
void setup_signals();

/**
 * @brief Clears the current shell line and reprints the prompt.
 */
void clear_line_and_prompt();

/**
 * @brief Adds a line to the in-memory command history buffer.
 * 
 * If the buffer is full, it shifts history up and appends the new line at the end.
 * 
 * @param line Input line to store in history.
 */
void add_to_history(const char* line);

/**
 * @brief Appends a command line to the history file on disk.
 * 
 * @param line Command line to save.
 */
void save_history_line(const char* line);

/**
 * @brief Loads history from the persistent history file into memory.
 */
void load_history();

/**
 * @brief Autocompletes a command based on current input using the command table.
 * 
 * @param buf Current input buffer.
 * @param buf_len Pointer to current buffer length.
 * @param cursor_pos Pointer to current cursor position.
 */
void autocomplete(char* buf, int* buf_len, int* cursor_pos);

/**
 * @brief Reads a full command line from user input with support for editing and history.
 * 
 * Supports features like:
 * - Arrow key navigation (up/down/left/right)
 * - Ctrl+A, Ctrl+E, Ctrl+K, Ctrl+U
 * - Backspace and tab-based autocomplete
 * 
 * @param buf Buffer to store user input.
 * @return Number of characters read, or -1 on EOF (e.g. Ctrl+D).
 */
int read_input_line(char* buf);

/**
 * @brief Restores terminal to canonical (cooked) mode with echoing.
 */
void disable_raw_mode();

/**
 * @brief Enables raw terminal mode (no echo, no line buffering).
 */
void enable_raw_mode();


#endif  // PENNSHELL_HELPER_H