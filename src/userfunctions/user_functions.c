#include "./user_functions.h"
#include "./util/p_errno.h"

void* u_cat(void* arg) {
  char** argv = (char**)arg;

  // Count argc
  int argc = 0;
  while (argv && argv[argc])
    argc++;

  // Directly call s_cat with original args — s_cat will call cat(argc, argv)
  s_cat((void*)argv);
  return NULL;
}

// Helper function to sleep for a specified number of seconds
void* u_sleep(void* arg) {
  char** t_args = (char**)arg;
  char** argv = t_args;
  if (!argv[1]) {
    panic("u_sleep: missing arguments");
    return NULL;
  }
  int seconds = atoi(argv[1]);
  int ticks = 10 * seconds;  // 10 ticks per second (1000ms/100ms)
  s_sleep(ticks);            //  sleep for the specified number of ticks
  return NULL;
}

void* u_busy(void* arg) {
  while (1)
    ;
  return NULL;
}

void* u_echo(void* arg) {
  s_echo(arg);  // call s_echo to get the output
  return NULL;
}

void* u_ls(void* arg) {
  s_ls(arg);
  return NULL;
}

void* u_touch(void* arg) {
  s_touch(arg);
  return NULL;
}

void* u_mv(void* arg) {
  s_mv(arg);
  return NULL;
}

void* u_cp(void* arg) {
  s_cp(arg);
  return NULL;
}

void* u_rm(void* arg) {
  s_rm(arg);
  return NULL;
}

void* u_chmod(void* arg) {
  s_chmod(arg);
  return NULL;
}

void* u_ps(void* arg) {
  s_ps();
  return NULL;
}

void* u_kill(void* arg) {
  char** t_args = (char**)arg;
  char** argv = t_args;

  int argc = 0;
  while (argv[argc] != NULL)
    argc++;

  if (argc < 2) {  // Check for at least one argument
    panic("Usage: kill [-term|-stop|-cont] pid1 pid2 ...");
  }
  int signal = P_SIGTERM;  // Default
  int start_index = 1;

  // Check if argv[1] is a signal
  if (strncmp(argv[1], "-", 1) == 0) {
    if (strcmp(argv[1], "-term") == 0) {
      signal = P_SIGTERM;
    } else if (strcmp(argv[1], "-stop") == 0) {
      signal = P_SIGSTOP;
    } else if (strcmp(argv[1], "-cont") == 0) {
      signal = P_SIGCONT;
    } else {
      panic("Invalid signal. Usage: kill [-term|-stop|-cont] pid1 pid2 ...");
    }
    start_index = 2;
  }

  if (start_index >= argc) {
    panic("Missing PID. Usage: kill [-term|-stop|-cont] pid1 pid2 ...");
  }

  for (int i = start_index; i < argc; ++i) {
    int pid = atoi(argv[i]);
    if (pid <= 0) {
      printf("Invalid PID: %s\n", argv[i]);
      continue;
    }
    // send signal
    if (s_kill(pid, signal) == -1) {
      u_perror("s_kill: invalid signal");
    }
  }
  return NULL;
}

void* zombie_child(void* arg) {
  // MMMMM Brains...!
  s_exit();
  return NULL;
}

void* zombify(void* arg) {
  thread_args_t* t_args = (thread_args_t*)arg;
  bool is_background = t_args->is_background;

  // Prepare the thread_args_t for the s_spawn call
  thread_args_t zombie_args = {
      .argv = (char*[]){"zombify_child", NULL},  // Arguments to pass
      .is_background = is_background             // Use the background flag
  };

  s_spawn(zombie_child, &zombie_args, 0, 1, 2, 1, P_BLOCKED, false,
          is_background);

  while (1)
    ;
  return NULL;
}

void* orphan_child(void* arg) {
  // Please sir,
  // I want some more
  while (1)
    ;
  s_exit();
  return NULL;
}

void* orphanify(void* arg) {
  thread_args_t* t_args = (thread_args_t*)arg;
  bool is_background = t_args->is_background;

  // Prepare the thread_args_t for the s_spawn call
  thread_args_t orphan_args = {
      .argv = (char*[]){"orphanify_child", NULL},  // Arguments to pass
      .is_background = is_background               // Use the background flag
  };

  s_spawn(orphan_child, &orphan_args, 0, 1, 2, 1, P_BLOCKED, false,
          is_background);
  return NULL;
}

void* u_nice(void* arg) {
  thread_args_t* t_args = (thread_args_t*)arg;
  char** argv = t_args->argv;
  bool is_background = t_args->is_background;
  if (!argv[1] || !argv[2]) {
    panic("Usage: nice <command> <priority>");
    return NULL;
  }

  char* command = argv[1];
  int priority = atoi(argv[2]);

  if (priority > 2  || priority < 0){
    k_print("Priority must be 0,1,2\n");
    return NULL;
  }

  // Look up the function in command_table
  void* (*func)(void*) = NULL;
  for (int i = 0; i < number_commands; ++i) {
    if (strcmp(command, command_table[i].name) == 0) {
      func = command_table[i].function;
      break;
    }
  }

  if (!func) {
    k_print("u_nice: unknown command '%s'\n", command);
    return NULL;
  }

  // Prepare arguments for the target command (argv[3] onward)
  int start = 3;  // command at argv[1], priority at argv[2]
  int count = 0;
  while (argv[start + count])
    count++;

  char** new_argv =
      malloc(sizeof(char*) * (count + 2));  // +1 for command, +1 for NULL
  new_argv[0] = strdup(command);            // include the command name itself
  for (int i = 0; i < count; i++) {
    new_argv[i + 1] = strdup(argv[start + i]);
  }
  new_argv[count + 1] = NULL;

  thread_args_t* child_args = malloc(sizeof(thread_args_t));
  child_args->argv = new_argv;
  child_args->is_background = is_background;

  s_spawn(func, child_args, 0, 1, 2, priority, P_BLOCKED, false, is_background);
  return NULL;
}

void* u_nice_pid(void* arg) {
  thread_args_t* t_args = (thread_args_t*)arg;
  char** argv = t_args->argv;

  if (!argv[1] || !argv[2]) {
    panic("Usage: nice_pid <priority> <pid>");
    return NULL;
  }

  int priority = atoi(argv[1]);
  int pid = atoi(argv[2]);

  if (pid <= 0 || priority < 0 || priority > 2) {
    panic("Invalid PID or priority. Priority must be 0, 1, or 2.\n");
    return NULL;
  }
  // Change the priority of the process
  if (s_nice(pid, priority) == -1) {
    k_print("s_nice: error");
  }

  return NULL;
}

void* u_man(void* arg) {
  printf("Available commands:\n");
  for (int i = 0; i < number_commands; ++i) {
    printf("  %-10s - %s\n", command_table[i].name,
           command_table[i].description);
  }
  return NULL;
}

void* u_bg(void* arg) {
  thread_args_t* t_args = (thread_args_t*)arg;
  char** argv = t_args->argv;

  int job_id = -1;  // Default: most recent job

  // Count arguments
  int argc = 0;
  while (argv[argc] != NULL)
    argc++;

  if (argc > 2) {
    panic("Incorrect Usage: bg [job_id]\n");
  }

  if (argc == 2) {
    job_id = atoi(argv[1]);
  }
  s_bg(job_id);
  return NULL;
}

pid_t u_fg(void* arg) {
  char** t_args = (char**)arg;
  char** argv = t_args;

  int job_id = -1;  // Default: most recent job

  // Count arguments
  int argc = 0;
  while (argv[argc] != NULL)
    argc++;

  if (argc > 2) {
    panic("Incorrect Usage: bg [job_id]\n");
  }

  if (argc == 2) {
    job_id = atoi(argv[1]);
  }

  pid_t answer = s_fg(job_id);
  return answer;
}

void* u_jobs(void* arg) {
  s_jobs();
  return NULL;
}

void* u_logout(void* arg) {
  exit(0);
  return NULL;
}

/************************************
 *       EXTRA CREDIT FUNCTIONS      *
 *************************************
 */

// Helper function to clear the screen
void* u_clear(void* arg) {
  s_clear();
  return NULL;
}

// Helper function to edit a file like VM
void* u_edit(void* arg) {
  s_edit((void*)arg);
  return NULL;
}

// Helper function to count lines, words, and characters in a file
void* u_wc(void* arg) {
  s_wc(arg);
  return NULL;
}