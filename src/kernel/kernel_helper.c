#include "./kernel_helper.h"
#include "./scheduler/scheduler_helper.h"
#include "./util/spthread.h"

// Suspend the current process
void k_proc_suspend() {
  spthread_suspend_self();
}

// Initialize a new process
int initialize_new_process(pcb_t* new_pcb,
                           pcb_t* parent,
                           char* argv[],
                           int priority,
                           int status,
                           bool is_background) {
  pcb_t* last_pcb = vec_get(&pcb_list, vec_len(&pcb_list) - 1);
  new_pcb->pid = last_pcb->pid + 1;  // increment the last PID
  new_pcb->job_id =
      parent->pid == 2 ? ++job_counter : parent->job_id;  // set job id
  new_pcb->ppid = parent->pid;
  new_pcb->priority = priority;
  new_pcb->cmd = argv[0];
  new_pcb->status = status ? status : P_BLOCKED;  // start as blocked
  new_pcb->wake_tick = 0;
  new_pcb->remaining_sleep_ticks = 0;
  new_pcb->is_background = is_background;  // is the pcb for a background job
  new_pcb->argv = argv;

  new_pcb->children =
      vec_new(INITIAL_VEC_CAPACITY, NULL);  // initialize children as empty

  if (parent) {
    proc_fd_ent* child_file_descriptor =
        (proc_fd_ent*)malloc(MAX_OPEN_FILES * sizeof(proc_fd_ent));
    memcpy(child_file_descriptor, parent->file_descriptors,
           MAX_OPEN_FILES * sizeof(proc_fd_ent));
    new_pcb->file_descriptors = child_file_descriptor;
  } else {
    new_pcb->file_descriptors = NULL;
  }

  // jobs are children of shell so add them to the job list
  if (parent->pid == 2) {
    vec_push_back(&job_list, new_pcb);
  }

  return 0;
}

// Get the PCB with a given PID
pcb_t* k_get_pcb_with_given_pid(int pid) {
  for (size_t i = 0; i < vec_len(&pcb_list); i++) {
    pcb_t* pcb = vec_get(&pcb_list, i);
    if (k_proc_getpid(pcb) == pid) {
      return pcb;
    }
  }
  return NULL;
}

// Add a child process to the init process PCB
void add_child_to_init_pcb(pcb_t* child) {
  pcb_t* init_pcb = k_get_pcb_with_given_pid(1);
  if (init_pcb == NULL) {
    panic("add_child_to_init_pcd: init process PCB is NULL");
    return;
  }
  vec_push_back(&init_pcb->children, child);
}

// Reparenting of children to init after parent dies
void reparent_children(pcb_t* parent) {
  pcb_t* init = k_get_pcb_with_given_pid(1);
  if (!init)
    panic("reparent_children: init process not found");

  for (size_t i = 0; i < vec_len(&parent->children); i++) {
    pcb_t* child = vec_get(&parent->children, i);
    if (child == NULL) {
      continue;
    }
    child->ppid = 1;       // reparent to init
    child->waited_by = 1;  // set the parent to init
    log_event("ORPHAN", "\t%d\t%d\t%s", child->pid, child->priority,
              child->cmd);  // log the event
    add_child_to_init_pcb(child);
    add_to_queue(init);
  }
  vec_clear(&parent->children);  // clear the children of the parent
}

// Remove a process from the PCB list
void remove_process_from_pcb(pcb_t* proc) {
  for (size_t i = 0; i < vec_len(&pcb_list); i++) {
    if (((pcb_t*)vec_get(&pcb_list, i))->pid == proc->pid) {
      vec_erase(&pcb_list, i);
      break;
    }
  }
}

// Remove a process from the background job list
void add_child_to_parent_pcb(pcb_t* parent, pcb_t* child) {
  if (parent && child) {
    vec_push_back(&parent->children, child);
  } else {
    panic("add_child_to_parent_pcb: parent or child is NULL");
  }
}

// Remove a child process from the parent process
int remove_child_from_parent_pcb(pcb_t* parent, pcb_t* child) {
  if (parent && child) {
    for (size_t i = 0; i < vec_len(&parent->children); i++) {
      if (vec_get(&parent->children, i) == child) {
        vec_erase(&parent->children, i);  // remove child from parent
        return 0;
      }
    }
  } else {
    panic("remove_child_from_parent_pcb: parent or child is NULL");
  }
  return -1;
}

// Remove a process from the job list only shallow copy so as to not delete the
// real pcb
void remove_process_pcb_from_job(pcb_t* proc) {
  for (size_t i = 0; i < vec_len(&job_list); i++) {
    if (((pcb_t*)vec_get(&job_list, i))->pid == proc->pid) {
      vec_shallow_erase(&job_list, i);
      break;
    }
  }
}

// Remove a process from the background job list
// This is a shallow copy so as to not delete the real PCB
void remove_process_pcb_from_background_job(pcb_t* child) {
  for (size_t i = 0; i < vec_len(&background_jobs); i++) {
    if (((pcb_t*)vec_get(&background_jobs, i))->pid == child->pid) {
      vec_shallow_erase(&background_jobs, i);
      break;
    }
  }
}

// Initialize the file descriptor table for the init process
void init_fd_table(pcb_t* init_pcb) {
  // Initialize file descriptor with stdin, stdout, stderr
  proc_fd_ent* init_fd =
      (proc_fd_ent*)malloc(MAX_OPEN_FILES * sizeof(proc_fd_ent));
  proc_fd_ent stdin;
  proc_fd_ent stdout;
  proc_fd_ent stderr;
  stdin =
      (proc_fd_ent){.proc_fd = 0, .mode = F_READ, .offset = 0, .global_fd = 0};
  stdout =
      (proc_fd_ent){.proc_fd = 1, .mode = F_WRITE, .offset = 0, .global_fd = 1};
  stderr =
      (proc_fd_ent){.proc_fd = 2, .mode = F_WRITE, .offset = 0, .global_fd = 2};
  init_fd[0] = stdin;
  init_fd[1] = stdout;
  init_fd[2] = stderr;
  for (int i = 3; i < MAX_OPEN_FILES; i++) {
    init_fd[i].proc_fd = -1;
  }
  init_pcb->file_descriptors = init_fd;
}