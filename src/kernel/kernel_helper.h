#ifndef _KERNEL_HELPER_H_
#define _KERNEL_HELPER_H_
#include "./kernel.h"

/**
 * @brief Suspend the current process.
 * This function will put the current process to sleep until it is resumed
 * by a signal or another process.
 */
void k_proc_suspend(void);

/**
 * @brief Initialize a new process.
 * This function sets up the new process's PCB with the given parameters.
 *
 * @param new_pcb Pointer to the new PCB.
 * @param parent Pointer to the parent PCB.
 * @param argv Arguments for the new process.
 * @param priority Priority of the new process.
 * @param status Status of the new process.
 * @param is_background Whether the process is a background job.
 * @return 0 on success, -1 on failure.
 */
int initialize_new_process(pcb_t* new_pcb,
                           pcb_t* parent,
                           char* argv[],
                           int priority,
                           int status,
                           bool is_background);

/**
 * @brief Get the PCB with a given PID.
 */
pcb_t* k_get_pcb_with_given_pid(int pid);

/**
 * @brief Add a child process to the parent process.
 *
 * @param parent Parent process.
 * @param child Child process.
 */
void reparent_children(pcb_t* parent);

/**
 * @brief Remove a child process from the parent process.
 *
 * @param proc Process to be removed from pcb.
 */
void remove_process_from_pcb(pcb_t* proc);
/**
 * @brief Remove a process from the job list.
 * @param proc Process to be removed from the job list.
 *
 */
void remove_process_pcb_from_job(pcb_t* proc);
/**
 * @brief Add a child process to the parent process.
 *
 * @param parent Parent process.
 * @param child Child process.
 */
void add_child_to_parent_pcb(pcb_t* parent, pcb_t* child);

/**
 * @brief Remove a child process from the parent process.
 *
 * @param parent Parent process.
 * @param child Child process.
 * @return 0 on success, -1 on failure.
 */
int remove_child_from_parent_pcb(pcb_t* parent, pcb_t* child);

/**
 * @brief Initialize the file descriptor table for the init process.
 *
 * @param init_pcb Pointer to the init process PCB.
 */
void init_fd_table(pcb_t* init_pcb);


/**
 * @brief Remove a process from the background job list.
 *
 * @param child Child process to be removed from the background job list.
 */
void remove_process_pcb_from_background_job(pcb_t* child);

#endif  // _KERNEL_HELPER_H_