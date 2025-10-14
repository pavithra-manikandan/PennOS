#include "./pennfat/pennfat.h"
#include "./util/spthread.h"
#include "./vec/Vec.h"

#ifndef PCB_H_
#define PCB_H_
#define INITIAL_NUM_PCB 1000
#define INITIAL_NUM_COMMANDS 100

// process_states
enum { P_RUNNING, P_STOPPED, P_BLOCKED, P_ZOMBIED };

// signal values
#define P_SIGSTOP 1
#define P_SIGCONT 2
#define P_SIGTERM 3
#define P_SIGEXIT 4
#define P_SIGQUIT 5

#define P_WIFEXITED(status) ((status) == P_SIGEXIT)
#define P_WIFSTOPPED(status) ((status) == P_SIGSTOP)
#define P_WIFSIGNALED(status) ((status) == P_SIGTERM)

typedef struct pcb_st {
  int pid;                        // process id
  int job_id;                     // job id
  int ppid;                       // parent process id
  int priority;                   // priority of the process
  struct spthread_st thread;      // thread of the process
  proc_fd_ent* file_descriptors;  // file descriptors
  int status;                     // status of the process
  char* cmd;                      // command name
  Vec children;                   // list of child processes
  int wake_tick;                  // tick to wake up
  int remaining_sleep_ticks;      // remaining sleep ticks
  int waited_by;                  // pid that is waiting for this process
  bool is_background;             // is this a background job?
  char** argv;                    // arguments to the command

} pcb_t;

extern Vec pcb_list;            // List of all PCBs
extern Vec background_jobs;     // Stores pointers to background job pcbs
extern Vec stopped_jobs;        // For stopped jobs to prioritize for `fg`
extern Vec job_list;            // List of all jobs
extern Vec sleeping_processes;  // List of sleeping pcb
#endif                          // PCB_H_
