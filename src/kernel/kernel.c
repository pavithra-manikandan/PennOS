#include "./kernel.h"
#include "./kernel_helper.h"
#include "./scheduler/scheduler_helper.h"
#include "./util/p_errno.h"

Vec pcb_list;            // Contains all the PCBs
Vec background_jobs;     // Contains all the background jobs
Vec stopped_jobs;        // Contains all the stopped jobs
Vec job_list;            // Contains all the jobs
Vec sleeping_processes;  // Contains all the sleeping processes
int job_counter = 2;     // Job ID starts from 2 (init and shell)

int current_tick = 0;  // Track tick count here

// Initialize all the lists as empty
void init_pcb_list() {
  pcb_list = vec_new(INITIAL_NUM_PCB, free);
  background_jobs = vec_new(INITIAL_NUM_PCB, free);
  stopped_jobs = vec_new(INITIAL_NUM_PCB, free);
  job_list = vec_new(INITIAL_NUM_PCB, free);
  sleeping_processes = vec_new(INITIAL_NUM_PCB, free);
}

// Initialize the kernel
void init_kernel() {
  init_pcb_list();
  pcb_t* init_pcb = malloc(sizeof(pcb_t));
  char* init_args[] = {"init", NULL};
  init_pcb->pid = 1;
  init_pcb->job_id = 0;
  init_pcb->ppid = 0;
  init_pcb->status = P_BLOCKED;
  init_pcb->priority = 0;
  init_pcb->cmd = "init";
  init_pcb->children = vec_new(INITIAL_VEC_CAPACITY, NULL);
  init_pcb->wake_tick = 0;
  init_pcb->remaining_sleep_ticks = 0;
  init_pcb->is_background = false;
  init_pcb->argv = init_args;
  init_fd_table(init_pcb);
  vec_push_back(&pcb_list, init_pcb);
  // create init thraed and pass the fucntion as k_reap_zombies_init which means
  // the init process will reap all the zombies
  spthread_create(&init_pcb->thread, NULL, (void*)k_reap_zombies_init, NULL);
  add_to_queue(init_pcb);
}

// Get the PCB with a given PID safely
int k_proc_getpid(pcb_t* proc) {
  if (proc == NULL) {
    return -1;
  }
  return proc->pid;
}

// s_spawn calls this function to create a new process
// This function is called by the kernel to create a new process
// It initializes the PCB and adds it to the PCB list
// It also sets the parent process and the file descriptors
pcb_t* k_proc_create(pcb_t* parent,
                     char* argv[],
                     int priority,
                     int status,
                     bool is_init,
                     bool is_background) {
  pcb_t* new_pcb = malloc(sizeof(pcb_t));
  if (initialize_new_process(new_pcb, parent, argv, priority, status,
                             is_background) != 0) {
    panic("k_proc_create: failed to initialize new process");
  }
  vec_push_back(&pcb_list, new_pcb);
  return new_pcb;
}

// Cleanup a terminated/finished thread's resources
void k_proc_cleanup(pcb_t* proc) {
  if (proc) {
    remove_process_pcb_from_job(proc);  // remove from job list
    remove_process_pcb_from_background_job(proc);
    free(proc->file_descriptors);
    remove_process_from_pcb(proc);  // remove from PCB list
    vec_destroy(&proc->children);
  } else {
    panic("k_proc_cleanup: proc is NULL\n");
  }
}

// Find the parent process of the current thread
pcb_t* find_parent_with_current_thread() {
  spthread_t self;
  if (!spthread_self(&self)) {
    panic("get_current_pcb: Failed to get current thread");
    return NULL;
  }
  for (size_t i = 0; i < vec_len(&pcb_list); i++) {
    pcb_t* pcb = vec_get(&pcb_list, i);
    // Check if the thread is equal to the current thread
    if (spthread_equal(self, pcb->thread)) {
      return pcb;
    }
  }
  return NULL;
}

// Fork a new child process
int k_fork(void* (*func)(void*),
           char* argv[],
           int fd0,
           int fd1,
           int parent_id,
           int priority,
           int status,
           bool is_init,
           bool is_background) {
  pcb_t* parent = NULL;
  if (is_init) {
    parent = k_get_pcb_with_given_pid(parent_id);  // get the parent process
  } else {
    parent = find_parent_with_current_thread();
  }
  pcb_t* child =
      k_proc_create(parent, argv, priority, status, is_init, is_background);
  spthread_create(&child->thread, NULL, func, argv);
  add_to_queue(child);
  add_child_to_parent_pcb(parent, child);  // add child to parent
  if (is_background) {
    // add child to background jobs
    vec_push_back(&background_jobs, child);

    k_print("[%d] %d \n", child->job_id,
                       child->pid);

  }
  return child->pid;
}

// The core of parent process - wait for the child process to finish
// This function is called by the kernel to wait for a child process to finish
int k_waitpid(int pid, int* wstatus, int nohang, bool is_init, int ppid) {
  pcb_t* parent;
  if (ppid > 0)
    parent = k_get_pcb_with_given_pid(ppid);
  else
    parent = find_parent_with_current_thread();
  if (!parent) {
    P_ERRNO = P_EWAITPID_I;
    return -1;
  }
  while (1) {
    Vec* children = &parent->children;
    if (vec_len(children) == 0) {
      P_ERRNO = P_EWAITPID_II;
      return -1;
    }

    for (size_t i = 0; i < vec_len(children); i++) {
      pcb_t* child = vec_get(children, i);
      if (!child)
        continue;
      if (pid != -1 && child->pid != pid)
        continue;
      if (child->status == P_ZOMBIED) {
        int reaped_pid = child->pid;  // reaped child
        if (wstatus)
          *wstatus = P_SIGEXIT;  // normal exit
        reparent_children(child);
        const char* event = is_init ? "WAITED (init)" : "WAITED";
        log_event(event, "\t%d\t%d\t%s", child->pid, child->priority,
                  child->cmd);                        // log the event
        remove_child_from_parent_pcb(parent, child);  // remove from parent
        k_proc_cleanup(child);  // free child and remove from PCB list
        return reaped_pid;
      }

      if (child->status == P_STOPPED) {
        if (wstatus)
          *wstatus = P_SIGSTOP;
        log_event("STOPPED", "\t%d\t%d\t%s", child->pid, child->priority,
                  child->cmd);  // log the event
        return child->pid;
      }
    }
    // deal with no hang
    if (nohang) {
      return 0;  // no child has exited so return
    }
    for (size_t i = 0; i < vec_len(children); i++) {
      pcb_t* child = vec_get(children, i);
      if (child) {
        child->waited_by =
            parent->pid;  // set the child's waited_by to the parent
      }
    }

    parent->status = P_BLOCKED;  // block the parent
    k_proc_suspend();            // suspend the parent
  }
  P_ERRNO = P_EWAITPID_III;
  return -1;
}

// Siganls for PennOS
int k_kill(int pid, int signal) {
  pcb_t* pcb_with_given_pid = k_get_pcb_with_given_pid(pid);
  if (pcb_with_given_pid == NULL) {
    return -1;
  }
  log_event("SIGNALED", "\t%d\t%d\t%s", pcb_with_given_pid->pid,
            pcb_with_given_pid->priority,
            pcb_with_given_pid->cmd);  // log the event
  if (signal < 0 || signal > 5) {
    panic("k_kill: signal must be between 0 and 4");
    return -1;
  }

  int killed = k_proc_kill(pcb_with_given_pid, signal);  // kill the process
  return killed;
}

void k_exit() {
  spthread_t self;
  if (!spthread_self(&self)) {
    panic("get_current_pcb: Failed to get current thread");
    return;
  }

  pcb_t* current_pcb =
      find_parent_with_current_thread();  // get the current process
  pcb_t* parent_pcb =
      k_get_pcb_with_given_pid(current_pcb->ppid);  // get the parent process
  if (current_pcb == NULL) {
    panic("k_exit: current process PCB is NULL");
    return;
  }
  // Mark the process as P_ZOMBIED (terminating state)
  current_pcb->status = P_ZOMBIED;
  log_event("ZOMBIE", "\t%d\t%d\t%s", current_pcb->pid, current_pcb->priority,
            current_pcb->cmd);  // log the event
  remove_pcb_from_queue(current_pcb,
                        current_pcb->priority);  // remove from queue
  if (parent_pcb == NULL) {
    panic("k_exit: parent processee PCB is NULL");
    return;
  }

  int parent_who_waited_on_this =
      current_pcb->waited_by;  // get the parent waiting on this
  pcb_t* waiting_parent =
      k_get_pcb_with_given_pid(parent_who_waited_on_this);  // get the parent

  if (waiting_parent && waiting_parent->status == P_BLOCKED) {
    add_to_queue(waiting_parent);
  }
  return;
}

// Change the priority of a process
int k_nice(int pid, int priority) {
  pcb_t* pcb_with_given_pid = k_get_pcb_with_given_pid(pid);
  if (pcb_with_given_pid == NULL) {
    P_ERRNO = P_EINVAL;
    return -1;
  }
  // Remove the PCB from its current priority queue
  int old_priority = pcb_with_given_pid->priority;
  remove_pcb_from_queue(pcb_with_given_pid, old_priority);
  pcb_with_given_pid->priority = priority;
  // Add the PCB to the new priority queue
  add_to_queue(pcb_with_given_pid);

  return 0;  // Success
}

// Sleep for a specified number of ticks
void k_sleep(unsigned int ticks) {
  if (ticks == 0)
    return;
  pcb_t* self = find_parent_with_current_thread();
  if (!self)
    panic("k_sleep: no current PCB");
  // log_event("BLOCKED", "\t%d\t%d\t%s", self->pid, self->priority, self->cmd);
  remove_pcb_from_queue(self, self->priority);
  self->status = P_BLOCKED;                // set the status to blocked
  self->wake_tick = current_tick + ticks;  // set the wake tick
  self->remaining_sleep_ticks = ticks;
  vec_push_back(&sleeping_processes, self);
  k_proc_suspend();  // suspend the process
}

// Send the signal to the process
int k_proc_kill(pcb_t* proc, int signal) {
  pcb_t* parent_pcb = k_get_pcb_with_given_pid(proc->ppid);
  switch (signal) {
    case P_SIGSTOP:  // Stop the process
      proc->status = P_STOPPED;
      log_event("STOPPED", "\t%d\t%d\t%s", proc->pid, proc->priority,
                proc->cmd);
      remove_from_queue(proc->priority);  // remove from queue

      // Check if it's sleeping and pause its timer
      for (int i = 0; i < vec_len(&sleeping_processes); i++) {
        if (vec_get(&sleeping_processes, i) == proc) {
          proc->remaining_sleep_ticks = proc->wake_tick - current_tick;
          vec_shallow_erase(&sleeping_processes,
                            i);  // remove from sleeping list
          break;
        }
      }

      vec_push_back(&stopped_jobs, proc);
      break;
    case P_SIGCONT:
      if (proc->remaining_sleep_ticks > 0) {
        proc->status = P_BLOCKED;
        proc->wake_tick = current_tick + proc->remaining_sleep_ticks;
        vec_push_back(&sleeping_processes, proc);
        proc->remaining_sleep_ticks = 0;
        remove_pcb_from_queue(proc, proc->priority);
      } else {
        proc->status = P_RUNNING;
        add_to_queue(proc);  // add to the queue
        log_event("CONTINUED", "\t%d\t%d\t%s", proc->pid, proc->priority,
                  proc->cmd);
      }
      break;
    case P_SIGTERM:
      // zombie it and have parent clean it up !!
      proc->status = P_ZOMBIED;
      log_event("ZOMBIE", "\t%d\t%d\t%s", proc->pid, proc->priority, proc->cmd);
      remove_pcb_from_queue(proc, proc->priority);
      int parent_who_waited_on_this = proc->waited_by;
      pcb_t* waiting_parent =
          k_get_pcb_with_given_pid(parent_who_waited_on_this);
      if (parent_who_waited_on_this != 0) {
        // If the parent is waiting on this child, wake it up
        waiting_parent->status = P_RUNNING;
        add_to_queue(waiting_parent);
      }
      k_proc_suspend();
      break;
    case P_SIGQUIT:
      proc->status = P_ZOMBIED;
      log_event("QUIT (core dumped)", "\t%d\t%d\t%s", proc->pid, proc->priority,
                proc->cmd);
      remove_pcb_from_queue(proc, proc->priority);
      int parent_waited_on_this = proc->waited_by;
      pcb_t* waiting_par = k_get_pcb_with_given_pid(parent_waited_on_this);
      if (parent_waited_on_this != 0) {
        // If the parent is waiting on this child, wake it up
        waiting_par->status = P_RUNNING;
        add_to_queue(waiting_par);
      }
      k_proc_suspend();
      break;
    default:
      P_ERRNO = P_EINVAL;
      return -1;
  }

  if (parent_pcb->status == P_BLOCKED) {
    parent_pcb->status = P_RUNNING;  // unblock the parent
    add_to_queue(parent_pcb);
  }
  return 0;
}

// Return the parent id safely
int k_proc_getppid(pcb_t* proc) {
  if (proc == NULL) {
    return -1;
  }
  return proc->ppid;
}

// print the jobs
void k_jobs() {
  for (size_t i = 0; i < vec_len(&job_list); i++) {
    pcb_t* current_job_pcb = vec_get(&job_list, i);

    // Skip NULL entries or processes that have been zombied and cleaned up
    if (!current_job_pcb || !current_job_pcb->cmd ||
        current_job_pcb->status == P_ZOMBIED) {
      continue;
    }
    k_print("[%d]  %d  ",
                       current_job_pcb->job_id, current_job_pcb->pid);

    for (int j = 0;
         current_job_pcb->argv[j] != NULL;
         j++) {
      k_print("%s ", current_job_pcb->argv[j]);
    }

    // Print status
    const char* status_str = "U\n";
    switch (current_job_pcb->status) {
      case P_RUNNING:
        status_str = "R\n";
        break;
      case P_STOPPED:
        status_str = "S\n";
        break;
      case P_BLOCKED:
        status_str = "B\n";
        break;
    }
    k_print("%s", status_str);


  }
}

// Print the list of processes
void k_ps() {
  k_print("List of processes:\n%-5s %-5s %-5s %-4s %-5s %s\n", "PID",
                     "PPID", "JOB", "PRI", "STAT", "CMD");

  for (size_t i = 0; i < vec_len(&pcb_list); i++) {
    pcb_t* pcb = vec_get(&pcb_list, i);
    if (pcb == NULL)
      continue;

    char* status_str = "?";
    switch (pcb->status) {
      case P_RUNNING:
        status_str = "R";
        break;
      case P_BLOCKED:
        status_str = "B";
        break;
      case P_STOPPED:
        status_str = "S";
        break;
      case P_ZOMBIED:
        status_str = "Z";
        break;
    }
    k_print("%-5d %-5d %-5d %-4d %-5s %s\n",
                   pcb->pid, pcb->ppid, pcb->job_id, pcb->priority, status_str,
                   pcb->cmd ? pcb->cmd : "(null)");
  }
}

int k_bg(int job_id) {
  pcb_t* target = NULL;

  // If job_id is provided
  if (job_id != -1) {
    for (int i = 0; i < vec_len(&pcb_list); i++) {
      pcb_t* job = vec_get(&pcb_list, i);
      if (job->job_id == job_id &&
          (job->status == P_RUNNING || job->status == P_STOPPED)) {
        target = job;
        break;
      }
    }
  } else {
    // Pick most recent stopped job
    if (!vec_is_empty(&stopped_jobs))
      target = vec_get(&stopped_jobs, vec_len(&stopped_jobs) - 1);
  }

  if (!target) {
    panic("bg: no such job\n");
    return 0;
  }
  add_to_queue(target);

  // Remove from stopped_jobs if it's there
  for (int i = 0; i < vec_len(&stopped_jobs); i++) {
    if (vec_get(&stopped_jobs, i) == target) {
      vec_shallow_erase(&stopped_jobs, i);
      break;
    }
  }

  // Push to background_jobs
  vec_push_back(&background_jobs, target);
  return k_kill(target->pid, P_SIGCONT);
}

// Bring a background job to the foreground
pid_t k_fg(int job_id) {
  pcb_t* target = NULL;

  // Check for job_id
  if (job_id != -1) {
    // int job_id = atoi((char*)arg);
    for (int i = 0; i < vec_len(&pcb_list); i++) {
      pcb_t* job = vec_get(&pcb_list, i);
      if (job->job_id == job_id &&
          (job->status == P_STOPPED || job->status == P_RUNNING)) {
        target = job;
        break;
      }
    }
  } else {
    // Most recent stopped job first
    if (!vec_is_empty(&stopped_jobs)) {
      target = vec_get(&stopped_jobs, vec_len(&stopped_jobs) - 1);
    } else if (!vec_is_empty(&background_jobs)) {
      target = vec_get(&background_jobs, vec_len(&background_jobs) - 1);
    }
  }

  if (!target) {
    panic("fg: no such job\n");
    return 0;
  }

  if (target->status == P_STOPPED) {
    for (int i = 0; i < vec_len(&stopped_jobs); i++) {
      if (vec_get(&stopped_jobs, i) == target) {
        vec_shallow_erase(&stopped_jobs, i);
        break;
      }
    }
  }

  // Remove from background_jobs
  for (int i = 0; i < vec_len(&background_jobs); i++) {
    if (vec_get(&background_jobs, i) == target) {
      vec_shallow_erase(&background_jobs, i);
      break;
    }
  }

  k_kill(target->pid, P_SIGCONT);
  return target->pid;
}

// Reap all zombie processes (shell)
void k_reap_zombies() {
  pcb_t* shell = vec_get(&pcb_list, 1);
  if (shell == NULL) {
    return;
  }
  Vec* children = &shell->children;
  int status;

  for (int i = 0; i < vec_len(children); i++) {
    pcb_t* child = vec_get(children, i);
    if (child && child->status == P_ZOMBIED) {
      k_waitpid(child->pid, &status, true, false, -1);  // wait for the child
    }
  }
}

// Reap all zombie processes (init)
void k_reap_zombies_init() {
  pcb_t* init = k_get_pcb_with_given_pid(1);
  if (!init) {
    panic("k_reap_zombies_init: init PCB is NULL");
  }

  while (1) {
    int status;
    init->status = P_BLOCKED;
    int cpid =
        k_waitpid(-1, &status, true, true, -1);  // noblocking, kernel mode
    if (cpid <= 0) {
      remove_pcb_from_queue(init,
                            init->priority);  // remove from queue if no child
      k_proc_suspend();
    }
  }
}

/************************************
 *       EXTRA CREDIT FUNCTIONS      *
 *************************************
 */

// Clear the screen and reprompt
void k_clear() {
  k_write(STDOUT_FILENO, CLEAR_SEQUENCE, strlen(CLEAR_SEQUENCE));
}

// Do a line, word and char count for the file
int k_wc(const char* filename,
         int* line_count,
         int* word_count,
         int* char_count) {
  if (!filename || !line_count || !word_count || !char_count) {
    return -1;  // Invalid arguments
  }

  int fd = k_open(filename, F_READ);
  if (fd < 0) {
    return fd;  // Could be FILE_NOT_FOUND or PERMISSION_DENIED
  }

  char buf[BUF_SIZE];
  int total_lines = 0, total_words = 0, total_chars = 0;
  int in_word = 0;

  while (1) {
    int bytes_read = k_read(fd, BUF_SIZE, buf);
    if (bytes_read < 0) {
      k_close(fd);
      return -1;
    }
    if (bytes_read == 0)
      break;

    for (int i = 0; i < bytes_read; i++) {
      char c = buf[i];
      total_chars++;

      if (c == '\n')
        total_lines++;

      if (c == ' ' || c == '\n' || c == '\t') {
        if (in_word) {
          total_words++;
          in_word = 0;
        }
      } else {
        in_word = 1;
      }
    }
  }

  // If the file ends with a non-whitespace character
  if (in_word) {
    total_words++;
  }

  // Set the output parameters
  *line_count = total_lines;
  *word_count = total_words;
  *char_count = total_chars;

  // Close the file descriptor
  k_close(fd);
  return 0;
}
