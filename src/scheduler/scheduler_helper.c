#include "scheduler_helper.h"

extern Vec background_jobs;

// Priority queues for the 3 levels
ThreadNode* priority_queues[3] = {NULL, NULL, NULL};

bool are_all_queues_empty(void) {
  return (!priority_queues[0] && !priority_queues[1] && !priority_queues[2]);
}

void remove_pcb_from_queue(pcb_t* pcb, int priority) {
  if (pcb == NULL) {
    panic("remove_pcb_from_queue: pcb is NULL");
    return;
  }
  if (priority < 0 || priority > 2) {
    panic("remove_pcb_from_queue: priority out of bounds");
    return;
  }

  if (!priority_queues[priority])
    return;  // Empty queue

  ThreadNode* prev = NULL;
  ThreadNode* current = priority_queues[priority];

  while (current) {
    if (current->pcb == pcb) {
      if (prev) {
        prev->next = current->next;
      } else {
        priority_queues[priority] = current->next;
      }
      free(current);
      return;
    }
    prev = current;
    current = current->next;
  }
}

void add_to_queue(pcb_t* pcb) {
  int priority = pcb->priority;
  ThreadNode* new_node = malloc(sizeof(ThreadNode));
  if (!new_node) {
    perror("Failed to allocate memory for thread node");
    exit(EXIT_FAILURE);
  }
  new_node->pcb = pcb;
  new_node->next = NULL;

  if (!priority_queues[priority]) {
    priority_queues[priority] = new_node;
  } else {
    ThreadNode* temp = priority_queues[priority];
    while (temp->next) {
      temp = temp->next;
    }
    temp->next = new_node;
  }
}

pcb_t* remove_from_queue(int priority) {
  if (!priority_queues[priority])
    return NULL;

  ThreadNode* temp = priority_queues[priority];
  pcb_t* pcb = temp->pcb;
  priority_queues[priority] = temp->next;
  free(temp);
  return pcb;
}

bool is_in_background_jobs(pcb_t* check_pcb) {
  for (int i = 0; i < vec_len(&background_jobs); i++) {
    if (((pcb_t*)vec_get(&background_jobs, i))->pid == check_pcb->pid) {
      return true;
    }
  }
  return false;
}

void idle_scheduler() {
  sigset_t suspend_set;
  sigfillset(&suspend_set);
  sigdelset(&suspend_set, SIGALRM);
  sigdelset(&suspend_set, SIGTSTP);
  sigsuspend(&suspend_set);
}
