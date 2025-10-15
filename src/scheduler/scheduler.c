#include "scheduler.h"
#include "scheduler_helper.h"

int schedule_index = 0;  // Global scheduling index
int running_pid = 0;     // Currently running process PID

// Scheduling ratio: 2.25x:1.5x:x => 9:6:4 (Priority 0:1:2)
static int priority_schedule[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0,  // Priority 0 (9x)
    1, 1, 1, 1, 1, 1,           // Priority 1 (6x)
    2, 2, 2, 2                  // Priority 2 (4x)
};

void run_scheduler() {
  // Check if all queues are empty
  if (are_all_queues_empty()) {
    idle_scheduler();
    return;
  }

  // Suspend current running thread and requeue if needed
  if (running_pid != 0) {
    pcb_t* current_pcb = NULL;

    for (int i = 0; i < vec_len(&pcb_list); ++i) {
      pcb_t* pcb = vec_get(&pcb_list, i);
      if (pcb && pcb->pid == running_pid && pcb->status == P_RUNNING) {
        current_pcb = pcb;
        break;
      }
    }

    if (current_pcb) {
      spthread_suspend(current_pcb->thread);
      if (current_pcb->status == P_RUNNING) {
        add_to_queue(current_pcb);
      }
    }
  }

  // Pick next PCB according to priority scheduling
  pcb_t* next_pcb = NULL;
  int attempts = 0;
  int selected_priority;

  while (!next_pcb && attempts < 19) {  // max 19 entries in priority_schedule
    selected_priority = priority_schedule[schedule_index];
    next_pcb = remove_from_queue(selected_priority);
    schedule_index = (schedule_index + 1) % 19;
    attempts++;
  }

  if (next_pcb) {
    running_pid = next_pcb->pid;
    next_pcb->status = P_RUNNING;
    log_event("SCHEDULE", "\t%d\t%d\t%s", running_pid, selected_priority,
              next_pcb->cmd);
    spthread_continue(next_pcb->thread);
  } else {
    running_pid = 0;
  }
}

void scheduler_tick(int signum) {
  current_tick++;
  log_tick();
  for (int i = 0; i < vec_len(&sleeping_processes); i++) {
    pcb_t* pcb = vec_get(&sleeping_processes, i);
    if (pcb->status == P_BLOCKED && pcb->wake_tick <= current_tick) {
      if (pcb->is_background || is_in_background_jobs(pcb)) {
        if (i == vec_len(&sleeping_processes) - 1) {
          k_print("[%d] + Done ", pcb->job_id);
        } else {
          k_print("[%d] Done ", pcb->job_id);
        }
        for (int j = 0; pcb->argv[j] != NULL; j++) {
          k_print("%s ", pcb->argv[j]);
        }
        // Print newline
        k_print("\n");

        // Print shell prompt
        k_print("%s", PROMPT);
      }

      pcb->status = P_RUNNING;
      add_to_queue(pcb);
      vec_shallow_erase(&sleeping_processes, i--);
    }
  }

  run_scheduler();
}

void scheduler_init() {
  struct sigaction sa;
  sa.sa_handler = scheduler_tick;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGALRM, &sa, NULL);

  struct itimerval timer;
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = QUANTUM;
  timer.it_interval = timer.it_value;
  setitimer(ITIMER_REAL, &timer, NULL);
}
