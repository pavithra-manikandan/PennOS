#include <string.h>
#include "./kernel/kernel.h"
#include "./kernel/kernel_helper.h"
#include "./pcb.h"
#include "./pennfat/pennfat.h"
#include "./util/panic.h"
#include "./util/spthread.h"
#include "./util/thread_args.h"
#ifndef SYSCALL_H_
#define SYSCALL_H_

/**
 * @brief Create a child process that executes the function `func`.
 * The child will retain some attributes of the parent.
 *
 * @param func Function to be executed by the child process.
 * @param argv Null-terminated array of args, including the command name as
 * argv[0].
 * @param fd0 Input file descriptor.
 * @param fd1 Output file descriptor.
 * @return pid_t The process ID of the created child process.
 */
pid_t s_spawn(void* (*func)(void*),
              thread_args_t* t_args,
              int fd0,
              int fd1,
              int pid,
              int priority,
              int status,
              bool is_init,
              bool is_background);

/**
 * @brief Wait on a child of the calling process, until it changes state.
 * If `nohang` is true, this will not block the calling process and return
 * immediately.
 *
 * @param pid Process ID of the child to wait for.
 * @param wstatus Pointer to an integer variable where the status will be
 * stored.
 * @param nohang If true, return immediately if no child has exited.
 * @param is_init If true, the process is the init process.
 * @param ppid Parent process ID.
 * @return pid_t The process ID of the child which has changed state on success,
 * -1 on error.
 */
pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang, bool is_init, int ppid);

/**
 * @brief Send a signal to a particular process.
 *
 * @param pid Process ID of the target proces.
 * @param signal Signal number to be sent.
 * @return 0 on success, -1 on error.
 */
int s_kill(pid_t pid, int signal);

/**
 * @brief Unconditionally exit the calling process.
 */
void s_exit(void);

/**
 * @brief List all processes on the system, displaying PID, PPID, priority,
 * status, and command name.
 */
void s_ps(void);

/**
 * @brief Brings a background job to the foreground.
 *
 * @param job_id ID of the job.
 * @return PID of the resumed foreground process.
 */
pid_t s_fg(int job_id);

/**
 * @brief Resumes a stopped job in the background.
 *
 * @param job_id ID of the job.
 * @return 0 on success, -1 on failure.
 */
int s_bg(int job_id);

/**
 * @brief Lists all jobs.
 *
 * Example Usage: jobs
 */
void s_jobs();

/**
 * @brief Set the priority of the specified thread.
 *
 * @param pid Process ID of the target thread.
 * @param priority The new priorty value of the thread (0, 1, or 2)
 * @return 0 on success, -1 on failure.
 */
int s_nice(pid_t pid, int priority);

/**
 * @brief Suspends execution of the calling proces for a specified number of
 * clock ticks.
 *
 * This function is analogous to `sleep(3)` in Linux, with the behavior that the
 * system clock continues to tick even if the call is interrupted. The sleep can
 * be interrupted by a P_SIGTERM signal, after which the function will return
 * prematurely.
 *
 * @param ticks Duration of the sleep in system clock ticks. Must be greater
 * than 0.
 */
void s_sleep(unsigned int ticks);

/**
 * @brief Retrieves the current thread's file descriptor table.
 *
 * @return Pointer to an array of proc_fd_ent structures.
 */
proc_fd_ent* get_file_descriptors();

/**
 *@brief Open a file name fname with the mode mode and return a file descriptor.
 *The allowed modes are as follows: F_WRITE - writing and reading, truncates if
 *the file exists, or creates it if it does not exist. Only one instance of a
 *file can be opened in F_WRITE mode; error if PennOS attempts to open a file in
 *F_WRITE mode more than once F_READ - open the file for reading only, return an
 *error if the file does not exist F_APPEND - open the file for reading and
 *writing but does not truncate the file if exists; additionally, the file
 *pointer references the end of the file.
 *
 * @param fname POSIX standard name of the file
 * @param mode mode to open the file in
 */
int s_open(const char* fname, int mode);

/**
 *@brief read n bytes from the file referenced by fd into buf.
 * On return, s_read returns the number of bytes read, 0 if EOF is reached, or a
 *negative number on error. Updates file descriptor metadata.
 *
 * @param fd file descriptor referencing the file
 * @param n number of bytes to read
 * @param buf char buffer to read into
 */
int s_read(int fd, int n, char* buf);

/**
 * @brief write n bytes from buf to the file referenced by fd, and increment the
 *file pointer by n Return the number of bytes written, or negative value on
 *error
 *
 * @param fd file descriptor referenceing the file
 *@param n number of bytes to write
 * @param buf char buffer to write from
 */
int s_write(int fd, int n, const char* str);

/**
 * @brief close the file fd and return 0 on success, or a negative value on
 * failure. On success the local process’ file descriptor table should be
 * cleaned up appropriately.
 *
 * @param fd file descriptor of the file to be closed
 */
int s_close(int fd);

/**
 * @brief remove the file. Be careful how you implement this, like Linux,
 * you should not be able to delete a file that is in use by another process.
 *
 * @param fname name of the file to be deleted.
 */
int s_unlink(const char* fname);

/**
 * @brief reposition the file pointer for fd to the offset relative to whence.
 *
 * @param fd file descriptor of the file to be traversed
 * @param offset offset within the file
 * @param whence
 */
int s_lseek(int fd, int offset, int whence);

int s_perm(const char* fname);
/**
 * @brief list file fname under the current directory.  If filename is NULL,
 * list all files in the current directory.
 *
 * @param fname name of the file
 */
void* s_ls(void* arg);

/**
 * @brief change the permission of the file fname to perm.
 * The permission is a number between 0 and 7, where 0 is no permission and 7 is
 * read, write, and execute.
 *
 * @param fname name of the file
 * @param perm permission to set
 */
void* s_touch(void* arg);

/**
 * @brief change the permission of the file fname to perm.
 * The permission is a number between 0 and 7, where 0 is no permission and 7 is
 * read, write, and execute.
 *
 * @param fname name of the file
 * @param perm permission to set
 */
void* s_rm(void* arg);

/**
 * @brief change the permission of the file fname to perm.
 * The permission is a number between 0 and 7, where 0 is no permission and 7 is
 * read, write, and execute.
 *
 * @param fname name of the file
 * @param perm permission to set
 */
void* s_mv(void* arg);

/**
 * @brief change the permission of the file fname to perm.
 * The permission is a number between 0 and 7, where 0 is no permission and 7 is
 * read, write, and execute.
 *
 * @param fname name of the file
 * @param perm permission to set
 */
void* s_cp(void* arg);

/**
 * @brief change the permission of the file fname to perm.
 * The permission is a number between 0 and 7, where 0 is no permission and 7 is
 * read, write, and execute.
 *
 * @param fname name of the file
 * @param perm permission to set
 */
void* s_cat(void* arg);

/**
 * @brief change the permission of the file fname to perm.
 * The permission is a number between 0 and 7, where 0 is no permission and 7 is
 * read, write, and execute.
 *
 * @param fname name of the file
 * @param perm permission to set
 */
void* s_chmod(void* arg);

/**
 * @brief Get the PCB of a process with a given PID.
 *
 * @param pid Process ID of the target process.
 * @return Pointer to the PCB of the specified process.
 */
pcb_t* s_get_pcb_with_given_pid(pid_t pid);

/**
 * @brief Reap zombie processes.
 *
 * This function will check for any zombie processes and clean them up.
 */
void s_reap_zombies();

/**
 * @brief change the permission of the file fname to perm.
 * The permission is a number between 0 and 7, where 0 is no permission and 7 is
 * read, write, and execute.
 *
 * @param fname name of the file
 * @param perm permission to set
 */
void* s_echo(void* arg);

/************************************
 *       EXTRA CREDIT FUNCTIONS      *
 *************************************
 */

void s_print(const char* fmt, ...);

/**
 * @brief Clear the screen.
 */
void* s_clear();

/**
 * @brief Opens a basic text editor interface for a given file.
 *
 * Supports commands like :w (save), :q (quit), :wq (save & quit),
 * :up (move cursor up), :down (move cursor down), :d (delete line),
 * and :n (insert new line).
 */
void* s_edit(void* arg);

/**
 * @brief Count the number of lines, words, and characters in a file.
 * @param arg Arguments for the function.
 *
 */
void* s_wc(void* arg);

/**
 * @brief Print formatted output to the console.
 *
 */
void s_printer(const char* fmt, ...);

#endif  // SYSCALL_H_