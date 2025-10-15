#ifndef SCHEDULER_HELPER_H
#define SCHEDULER_HELPER_H

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "./vec/Vec.h"
#include "kernel/kernel.h"
#include "log.h"
#include "pcb.h"
#include "util/panic.h"

/**
 * @brief Node structure used for process priority queues.
 *
 * Each node holds a pointer to a PCB and a pointer to the next node.
 */
typedef struct ThreadNode {
  pcb_t* pcb;               // Pointer to the process control block.
  struct ThreadNode* next;  // Pointer to the next node in the queue.
} ThreadNode;

/**
 * @brief Adds a process (represented by its PCB) to the corresponding priority
 * queue.
 *
 * This function inserts the process into the priority queue based on its
 * priority level.
 *
 * @param pcb A pointer to the process control block (PCB) of the process to be
 * added.
 */
void add_to_queue(pcb_t* pcb);

/**
 * @brief Removes and returns the next process from a given priority queue.
 *
 * @param priority The priority level (0=highest, 2=lowest) to remove from.
 * @return pcb_t* Pointer to the removed PCB, or NULL if the queue is empty.
 */
pcb_t* remove_from_queue(int priority);

/**
 * @brief Removes a specific process from its priority queue.
 *
 * Searches for the PCB in the priority queue and removes it if found.
 *
 * @param pcb A pointer to the PCB to be removed.
 * @param priority The priority level from which to remove the PCB.
 */
void remove_pcb_from_queue(pcb_t* pcb, int priority);

/**
 * @brief Checks if all priority queues are empty.
 *
 * @return true if all queues are empty, false otherwise.
 */
bool are_all_queues_empty(void);

/**
 * @brief Checks if a process is in the background jobs list.
 *
 * @param check_pcb A pointer to the PCB to check.
 * @return true if the process is in background jobs, false otherwise.
 */
bool is_in_background_jobs(pcb_t* check_pcb);

/**
 * @brief Puts the CPU into an idle state, waiting for signals.
 *
 * Called when there are no runnable processes.
 */
void idle_scheduler(void);

#endif  // SCHEDULER_HELPER_H
