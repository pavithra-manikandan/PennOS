#ifndef KERNEL_H_
#define KERNEL_H_

#include <pcb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./kfat_helper.h"
#include "./pennfat/pennfat.h"
#include "./scheduler/log.h"
#include "./scheduler/scheduler.h"
#include "./shell/pennshell.h"
#include "./syscall/sys_call.h"
#include "./userfunctions/user_functions.h"
#include "./util/command_table.h"
#include "./util/panic.h"
#include "./util/parser.h"
#include "./util/thread_args.h"

#define INITIAL_VEC_CAPACITY 4
#define BUF_SIZE 512
#define PRINT_BUFFER_SIZE 256
#define PROMPT "penn-os> "
#define CLEAR_SEQUENCE "\x1b[2J\x1b[H"

extern int job_counter;
/*
 * @brief Initialize the kernel.
 * This includes initializing the PCB list and creating the INIT process.
 */
void init_kernel();

/**
 * @brief Find the parent process of the current thread.
 *
 * @return Pointer to the parent PCB.
 */
pcb_t* find_parent_with_current_thread();

/**
 * @brief Create a new process.
 *
 * @param parent Parent process.
 * @param argv Arguments for the new process.
 * @param priority Priority of the new process.
 * @param status Status of the new process.
 * @param is_init Whether the new process is the init process.
 * @param is_background Whether the new process is a background job.
 * @return Pointer to the newly created PCB.
 */
pcb_t* k_proc_create(pcb_t* parent,
                     char* argv[],
                     int priority,
                     int status,
                     bool is_init,
                     bool is_background);

/**
 * @brief Clean up a terminated/finished thread's resources.
 * This may include freeing the PCB, handling children, etc.
 */
void k_proc_cleanup(pcb_t* proc);

/**
 * @brief Fork a new child process.
 *
 * @param func Function to execute in the child process.
 * @param argv Arguments to pass to the function.
 * @param fd0 File descriptor 0.
 * @param fd1 File descriptor 1.
 * @param pid Parent process ID.
 * @param priority Priority of the child process.
 * @param status Status of the child process.
 * @param is_init Whether the process is the init process.
 * @param is_background Whether the process is a background job.
 * @return PID of the child process.
 */
int k_fork(void* (*func)(void*),
           char* argv[],
           int fd0,
           int fd1,
           int pid,
           int priority,
           int status,
           bool is_init,
           bool is_background);

/**
 * @brief Wait for a child process to finish.
 *
 * @param pid PID of the child process.
 * @param wstatus Status of the child process.
 * @param nohang Whether to wait indefinitely.
 * @param is_init Whether the process is the init process.
 * @param ppid Parent process ID.
 * @return PID of the child process or -1 on error.
 */
int k_waitpid(int pid, int* wstatus, int nohang, bool is_init, int ppid);

/**
 * @brief Reap zombie processes.
 *
 * @param pid PID of the process to reap.
 * @return 0 on success, -1 on failure.
 */
int k_reap_zombies_cleanup(int pid);

/**
 * @brief Kill a process.
 *
 * @param pid PID of the process to kill.
 * @param signal Signal to send to the process.
 * @return 0 on success, -1 on failure.
 */
int k_kill(int pid, int signal);

/**
 * @brief Exit the current process.
 */
void k_exit();

/**
 * @brief Change the priority of a process.
 *
 * @param pid PID of the process.
 * @param priority New priority.
 * @return 0 on success, -1 on failure.
 */
int k_nice(int pid, int priority);

/**
 * @brief Sleep for a specified number of ticks.
 *
 * @param ticks Number of ticks to sleep.
 */
void k_sleep(unsigned int ticks);

/**
 * @brief Kill a process.
 *
 * @param proc Process to kill.
 * @param signal Signal to send.
 * @return 0 on success, -1 on failure.
 */
int k_proc_kill(pcb_t* proc, int signal);

/**
 * @brief Get the PID of the current process.
 *
 * @return PID of the current process.
 */
int k_proc_getpid(pcb_t* proc);

/**
 * @brief Get the PID of the parent process.
 *
 * @return PID of the parent process.
 */
int k_proc_getppid(pcb_t* proc);
/**
 * @brief List all processes on PennOS, displaying PID, PPID, priority, status,
 * and command name.
 */
void k_ps();

/**
 * @brief List all jobs on PennOS, displaying job ID, PID, and command name.
 */
void k_jobs();

/**
 * @brief Bring a background job to the background.
 * @param job_id Job ID of the background job.
 * If nothing, -1 is passed, the most recent stopped job is brought to the
 * background.
 * @return 0 on success, -1 on failure.
 */
int k_bg(int job_id);

/**
 * @brief Bring a background job to the foreground.
 *
 * @param job_id Job ID of the background job.
 * If nothing, -1 is passed, the most recent stopped job is brought to the
 * foreground.
 * @return 0 on success, -1 on failure.
 */
int k_fg(int job_id);

/**
 * @brief Reap zombie processes (shell and kernel).
 */
void k_reap_zombies();
/**
 * @brief Initialize the zombie reaping process for INIT process.
 */
void k_reap_zombies_init();

/**
 * @brief Clear the screen.
 */
void k_clear();

/**
 * @brief Finds an already open file descriptor for the given file and mode.
 *
 * Increments the reference count if the file is already open.
 * Validates permissions before returning the open file descriptor index.
 *
 * @param fname Name of the file to find.
 * @param mode Access mode (F_READ, F_WRITE, or F_APPEND).
 * @return File descriptor index if found and permitted, PERMISSION_DENIED on
 * access error, or -1 if not open.
 */
int find_open_fd(const char* fname, int mode);

/**
 * @brief Finds an existing directory entry or creates a new one if allowed.
 *
 * If the file does not exist and mode is F_WRITE, this function creates a new
 * directory entry and allocates its first block. It expands the root directory
 * if necessary to accommodate new entries.
 *
 * @param fname Name of the file.
 * @param mode File open mode (only F_WRITE supports creation).
 * @return Pointer to the directory entry, or NULL on failure or permission
 * denial.
 */
dir_entry_t* find_or_create_entry(const char* fname, int mode);

/**
 * @brief Allocates a new file descriptor slot in the open file table.
 *
 * Sets the file descriptor fields based on the provided directory entry and
 * mode. If the mode is F_WRITE, clears existing FAT chain and resets the file
 * size.
 *
 * @param entry Pointer to the directory entry.
 * @param mode Access mode to open the file with.
 * @return File descriptor index on success, -1 if no free slot is available.
 */
int allocate_fd(dir_entry_t* entry, int mode);

/**
 * @brief Calculate the number of lines, words, and characters in a file.
 *
 * @param filename Path to the file.
 *   @param line_count Pointer to store the number of lines.
 * @param word_count Pointer to store the number of words.
 *   @param char_count Pointer to store the number of characters.
 * @return 0 on success, -1 on failure.
 */
int k_wc(const char* filename,
         int* line_count,
         int* word_count,
         int* char_count);

#endif  // KERNEL_H_