#include "./kernel/kernel.h"

#ifndef USER_FUN_H_
#define USER_FUN_H
#define MAX_BUFFER_SIZE 1024

/**
 * @brief Executes `cat` to display or redirect file contents.
 */
void* u_cat(void* arg);

/**
 * @brief Sleeps for a specified number of seconds (converted to ticks).
 */
void* u_sleep(void* arg);

/**
 * @brief Creates an infinite CPU-busy loop.
 */
void* u_busy(void* arg);

/**
 * @brief Prints arguments to stdout with spacing and a newline.
 */
void* u_echo(void* arg);

/**
 * @brief Lists files and directories (unimplemented).
 */
// void* u_ls(void* arg);

/**
 * @brief For each file, create an empty file if it doesn't exist, else update
 * its timestamp.
 *
 * Example Usage: touch f1 f2 f3 f4 f5
 */
void* u_touch(void* arg);

/**
 * @brief Rename a file. If the `dst_file` file already exists, overwrite it.
 *
 * Print appropriate error message if:
 * - `src_file` is not a file that exists
 * - `src_file` does not have read permissions
 * - `dst_file` file already exists but does not have write permissions
 *
 * Example Usage: mv src_file dst_file
 */
void* u_mv(void* arg);

/**
 * Copy a file. If the `dst_file` file already exists, overwrite it.
 *
 * Print appropriate error message if:
 * - `src_file` is not a file that exists
 * - `src_file` does not have read permissions
 * - `dst_file` file already exists but does not have write permissions
 *
 * Example Usage: cp src_file dst_file
 */
void* u_cp(void* arg);

/**
 * @brief Remove a list of files.
 * Treat each file in the list as a separate transaction. (i.e. if removing
 * file1 fails, still attempt to remove file2, file3, etc.)
 *
 * Print appropriate error message if:
 * - `file` is not a file that exists
 *
 * Example Usage: rm f1 f2 f3 f4 f5
 */
void* u_rm(void* arg);

/**
 * @brief Change permissions of a file.
 * There's no need to error if a permission being added already exists, or
 * if a permission being removed is already not granted.
 *
 * Print appropriate error message if:
 * - `file` is not a file that exists
 * - `perms` is invalid
 *
 * Example Usage: chmod +x file (adds executable permission to file)
 * Example Usage: chmod +rw file (adds read + write permissions to file)
 * Example Usage: chmod -wx file (removes write + executable permissions from
 * file)
 */
void* u_chmod(void* arg);

/**
 * @brief List all processes on PennOS, displaying PID, PPID, priority, status,
 * and command name.
 *
 * Example Usage: ps
 */
void* u_ps(void* arg);

/**
 * @brief Sends a specified signal to a list of processes.
 * If a signal name is not specified, default to "term".
 * Valid signals are -term, -stop, and -cont.
 *
 * Example Usage: kill 1 2 3 (sends term to processes 1, 2, and 3)
 * Example Usage: kill -term 1 2 (sends term to processes 1 and 2)
 * Example Usage: kill -stop 1 2 (sends stop to processes 1 and 2)
 * Example Usage: kill -cont 1 (sends cont to process 1)
 */
void* u_kill(void* arg);

/**
 * @brief Helper for zombify.
 */
void* zombie_child(void* arg);
/**
 * @brief Used to test zombifying functionality of your kernel.
 *
 * Example Usage: zombify
 */
void* zombify(void* arg);

/**
 * @brief Helper for orphanify.
 */
void* orphan_child(void* arg);

/**
 * @brief Used to test orphanifying functionality of your kernel.
 *
 * Example Usage: orphanify
 */
void* orphanify(void* arg);

// shell built-ins should be run as the shell - subroutines!!
/**
 * @brief Spawn a new process for `command` and set its priority to `priority`.
 * 2. Adjust the priority level of an existing process.
 *
 * Example Usage: nice 2 cat f1 f2 f3 (spawns cat with priority 2)
 */
void* u_nice(void* arg);

/**
 * @brief Adjust the priority level of an existing process.
 *
 * Example Usage: nice_pid 0 123 (sets priority 0 to PID 123)
 */
void* u_nice_pid(void* arg);

/**
 * @brief Lists all available commands.
 *
 * Example Usage: man
 */
void* u_man(void* arg);

/**
 * @brief Resumes the most recently stopped job in the background, or the job
 * specified by job_id.
 *
 * Example Usage: bg
 * Example Usage: bg 2 (job_id is 2)
 */
void* u_bg(void* arg);

/**
 * @brief Brings the most recently stopped or background job to the foreground,
 * or the job specified by job_id.
 *
 * Example Usage: fg
 * Example Usage: fg 2 (job_id is 2)
 */
pid_t u_fg(void* arg);

/**
 * @brief Lists all jobs.
 *
 * Example Usage: jobs
 */
void* u_jobs(void* arg);

/**
 * @brief Exits the shell and shutsdown PennOS.
 *
 * Example Usage: logout
 */
void* u_logout(void* arg);

/************************************
 *       EXTRA CREDIT FUNCTIONS      *
 *************************************
 */

/**
 * @brief Clears the screen.
 *
 * Example Usage: logout
 */
void* u_clear(void* arg);

/**
 * @brief Edit a file and act like VM
 *
 */
void* u_edit(void* arg);

/**
 * @brief Count the number of lines, words, and characters in a file.
 */
void* u_wc(void* arg);
#endif  // USER_FUN_H_