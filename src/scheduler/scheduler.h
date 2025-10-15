#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "./util/spthread.h"
#include "./vec/Vec.h"
#include "kernel/kernel.h"
#include "log.h"
#include "pcb.h"

#define QUANTUM 100000  // 100ms in microseconds

extern int current_tick;

/**
 * @brief Initializes the scheduler's timer and sets up the tick handler.
 *
 * Sets a periodic timer interrupt using SIGALRM that triggers the scheduler
 * every QUANTUM microseconds.
 */
void scheduler_init(void);

/**
 * @brief Picks and runs the next process from the priority queues.
 *
 * Suspends the currently running process if necessary, and schedules a new
 * process based on the configured priority ratios.
 */
void run_scheduler(void);

/**
 * @brief Handles timer ticks for the scheduler.
 *
 * Called on each SIGALRM signal to update sleeping processes, wake them if
 * needed, and invoke the scheduler to select the next runnable process.
 *
 * @param signum The signal number (expected to be SIGALRM).
 */
void scheduler_tick(int signum);

#endif // SCHEDULER_H
