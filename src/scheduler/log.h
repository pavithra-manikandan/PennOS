#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

// Initialize logging system with a filename (default if NULL)
/**
 * @brief Initializes the logging system by opening a log file.
 * 
 * If a filename is provided, the log will be written to that file. If NULL is passed, 
 * a default file is used. If the file cannot be opened, the program will terminate.
 * 
 * @param filename The name of the log file. If NULL, a default filename is used.
 */
void log_init(const char* filename);

/**
 * @brief Logs an event with a variable number of arguments.
 * 
 * This function writes a log entry with the specified operation and associated arguments 
 * in a formatted manner. The log format will adhere to the specific structure required 
 * (e.g., for scheduling events, process events, etc.).
 * 
 * @param operation A string representing the type of operation being logged.
 * @param arg_count The number of arguments to log.
 * @param ... The arguments themselves that will be formatted into the log.
 */
void log_event(const char* operation, const char* fmt, ...);

/**
 * @brief Increments clock tick counter (called in scheduler tick).
 * 
 * This function is called every time the scheduler ticks, logging the current clock tick.
 * 
 * @note This function is used to track the time in the OS and is linked to scheduler activity.
 */
void log_tick();

/**
 * @brief Closes the log file.
 * 
 * This function closes the log file and frees any resources associated with it. It is important 
 * to call this function once logging is no longer required.
 */
void log_close();

#endif // LOG_H
