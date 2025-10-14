#include "./pennshell.h"
#include <termios.h>
#include "./pennshell_helper.h"
#include "./util/p_errno.h"
#include "./syscall/sys_call.h"

// Global variables
struct parsed_command* cmd;  // Parsed command structure
int read_len = -1;           // Length of the read command

static void* (*thread_func_to_run)(void*) =
    NULL;                  // Function to run in the thread
bool aio_enabled = false;  // Asynchronous I/O enabled

/// Function to handle the shell prompt
void* wrapper(void* arg) {
  thread_args_t* targs = (thread_args_t*)arg;
  void* ret = NULL;
  if (thread_func_to_run) {
    ret = thread_func_to_run(targs);
  }
  s_exit();  // Exit the thread always after running the function
  return ret;
}

proc_fd_ent stdin_proc_fd =
    (proc_fd_ent){.proc_fd = 0, .mode = F_READ, .offset = 0, .global_fd = 0};

proc_fd_ent stdout_proc_fd =
    (proc_fd_ent){.proc_fd = 1, .mode = F_WRITE, .offset = 0, .global_fd = 1};

// Function to initialize the shell
void init_shell() {
  if (!aio_enabled) {
    enable_raw_mode();
    atexit(disable_raw_mode);
  }
  setup_signals();  // Register signal handlers
  cmd = NULL;
}

// Function to reset redirections
int redirect_stdout(struct parsed_command* command,
                    proc_fd_ent* file_descriptors) {
  int stdout_file_fd = -1;
  if (cmd->is_file_append) {
    stdout_file_fd = s_open(cmd->stdout_file, F_APPEND);
    if (stdout_file_fd == -1) {
      u_perror("s_open: Failed");
    }
  } else {
    stdout_file_fd = s_open(cmd->stdout_file, F_WRITE);
    if (stdout_file_fd == -1) {
      u_perror("s_open: Failed");
    }
  }
  if (stdout_file_fd < 0) {
    k_print("[s-open] %s failed %s\n",
                       cmd->stdout_file, cmd->commands[0][0]);

    return stdout_file_fd;
  } else {
    // Redirection
    file_descriptors[1] = file_descriptors[stdout_file_fd];
    file_descriptors[1].proc_fd = 1;
  }
  return stdout_file_fd;
}

// Function to redirect stdin
int redirect_stdin(struct parsed_command* command,
                   proc_fd_ent* file_descriptors) {
  int stdin_file_fd = s_open(cmd->stdin_file, F_READ);
  if (stdin_file_fd == -1) {
    u_perror("s_open: Failed");
  }
  if (stdin_file_fd < 0) {
    if (file_descriptors[STDOUT_FILENO].global_fd != 1) {
      if (s_close(STDOUT_FILENO) == -1) {
        u_perror("s_close: Failed");
      }
      file_descriptors[STDOUT_FILENO] = stdout_proc_fd;
    }

  } else {
    // Redirect stdin to stdin_file_fd
    file_descriptors[0] = file_descriptors[stdin_file_fd];
    file_descriptors[0].proc_fd = 0;
  }
  return stdin_file_fd;
}

void process_script_lines(int script_fd) {
  char script_line[4096];
  int line_len = 0;
  while (s_read(script_fd, 1, script_line + line_len) > 0) {
    if (script_line[line_len] == '\n' || script_line[line_len] == ';') {
      script_line[strcspn(script_line, "\n")] = 0;
      struct parsed_command* script_cmd;
      int example = parse_command(script_line, &script_cmd);
      print_parsed_command(script_cmd);
      if (example == -1 || script_cmd == NULL) {
        s_print("invalid command : Encountered a system call error\n");
        free(script_cmd);
        script_cmd = NULL;
        continue;
      }
      if (example > 0) {
        k_print("invalid: Parser error: %d\n", example);
        free(script_cmd);
        script_cmd = NULL;
        continue;
      }
      execute_script_command(script_cmd);
      free(script_cmd);
      line_len = 0;
    } else {
      line_len++;
    }
  }
}

void execute_script_command(struct parsed_command* script_cmd) {
  char** cleaned_argv = malloc(sizeof(char*) * 100);  // adjust size if needed
  int cleaned_argc = 0;
  char** original_args = script_cmd->commands[0];

  for (int i = 0; original_args[i]; i++) {
    if ((strcmp(original_args[i], ">") == 0 ||
         strcmp(original_args[i], ">>") == 0) &&
        original_args[i + 1]) {
      s_open(original_args[i + 1],
             strcmp(original_args[i], ">>") == 0 ? F_APPEND : F_WRITE);
      i++;  // skip next token
    } else if (strcmp(original_args[i], "<") == 0 && original_args[i + 1]) {
      s_open(original_args[i + 1], F_READ);
      i++;
    } else {
      cleaned_argv[cleaned_argc++] = strdup(original_args[i]);
    }
  }
  cleaned_argv[cleaned_argc] = NULL;

  const char* input_cmd = cleaned_argv[0];

  for (int i = 0; i < number_commands; ++i) {
    if (strcmp(input_cmd, command_table[i].name) == 0) {
      thread_func_to_run = command_table[i].function;

      thread_args_t* targs = malloc(sizeof(thread_args_t));
      targs->argv = cleaned_argv;
      targs->is_background = script_cmd->is_background;

      if (command_table[i].is_builtin) {
        command_table[i].function(targs);
      } else {
        proc_fd_ent* file_descriptors = get_file_descriptors();
        int stdout_fd = -1, stdin_fd = -1;
        if (script_cmd->stdout_file) {
          stdout_fd = redirect_stdout(script_cmd, file_descriptors);
          if (stdout_fd < 0)
            return;
        }
        if (script_cmd->stdin_file) {
          stdin_fd = redirect_stdin(script_cmd, file_descriptors);
          if (stdin_fd < 0)
            return;
        }

        current_foreground_pid = s_spawn(wrapper, targs, 0, 1, 2, 1, P_BLOCKED,
                                         false, script_cmd->is_background);

        if (current_foreground_pid == -1) {
          char buf[PRINT_BUFFER_SIZE];
          k_print("s_spawn: failed to fork %s ", input_cmd);
          u_perror(buf);
        }

        if (!script_cmd->is_background) {
          int wstatus;
          if (s_waitpid(current_foreground_pid, &wstatus, false, false, -1) ==
              -1) {
            char buf[PRINT_BUFFER_SIZE];
            k_print("Failed to waitpid for %s ", input_cmd);
            u_perror(buf);
          }
        }

        reset_redirections();  // Reset file descriptors
      }
      break;
    }
  }
}

// Function to reset redirections
void reset_redirections() {
  proc_fd_ent* file_descriptors = get_file_descriptors();
  file_descriptors[0] = stdin_proc_fd;
  file_descriptors[1] = stdout_proc_fd;
}

// Function to handle the shell prompt
void* penn_shell(void* arg) {
  char buf[128];
  char msg[PRINT_BUFFER_SIZE];
  while (1) {
    memset(buf, 0, sizeof(buf));      // Clear the buffer
    read_len = read_input_line(buf);  // Read input line
    if (read_len == -1) {
      punmount();  // Unmount the filesystem
      k_print("\nExiting penn-os...\n");

      exit(0);  // Exit the shell
    }
    if (read_len > 0)
      add_to_history(buf);                   // Add to history if not empty
    int example = parse_command(buf, &cmd);  // Parse the command
    buf[strcspn(buf, "\n")] = 0;
    if (example == -1 || cmd == NULL) {
      int len = snprintf(msg, sizeof(msg), "invalid command : Encountered a system call error\n");
      s_write(STDERR_FILENO, len, msg);
      free(cmd);
      cmd = NULL;
      continue;
    }
    if (example > 0) {
      int len = snprintf(msg, sizeof(msg), "invalid: Parser error: %d\n", example);
      s_write(STDERR_FILENO, len, msg);
      free(cmd);
      cmd = NULL;
      continue;
    }

    if (cmd->num_commands > 0 && cmd->commands[0][0]) {
      const char* input_cmd = cmd->commands[0][0];
      thread_func_to_run = NULL;
      bool command_found = false;
      for (int i = 0; i < number_commands;
           ++i) {  // Check if command is in the table
        if (strcmp(input_cmd, command_table[i].name) == 0) {
          command_found = true;
          break;
        }
      }

      // Check if the command is fg
      if (!command_found && strcmp(input_cmd, "fg") != 0) {
        int perm = s_perm(input_cmd);
        if (perm != PERM_ALL && perm != PERM_EXEC && perm != PERM_READ_EXEC) {
          int len = snprintf(msg, sizeof(msg), "command not found: %s\n", input_cmd);
          s_write(STDOUT_FILENO, len, msg);
          free(cmd);
          cmd = NULL;
          continue;
        }
      }

      // REDIRECTION HANDLING
      int stdin_fd = 0;
      int stdout_fd = 1;
      int cleaned_argc = 0;
      char** original_args = cmd->commands[0];

      // Make a cleaned argv without redirection tokens
      char** cleaned_argv = malloc(sizeof(char*) * 100);  // adjust if needed

      for (int i = 0; original_args[i]; i++) {
        if ((strcmp(original_args[i], ">") == 0 ||
             strcmp(original_args[i], ">>") == 0) &&
            original_args[i + 1]) {
          stdout_fd =
              s_open(original_args[i + 1],
                     strcmp(original_args[i], ">>") == 0 ? F_APPEND : F_WRITE);
          i++;  // skip next token
        } else if (strcmp(original_args[i], "<") == 0 && original_args[i + 1]) {
          stdin_fd = s_open(original_args[i + 1], F_READ);
          i++;
        } else {
          cleaned_argv[cleaned_argc++] = strdup(original_args[i]);
        }
      }
      cleaned_argv[cleaned_argc] = NULL;
      // Check if the command is "cat" and handle it separately
      if (strcmp("cat", input_cmd) == 0) {
        cleaned_argv = cmd->commands[0];
      }

      // Check if the command is "fg"
      if (strcmp(input_cmd, "fg") == 0) {
        thread_args_t* targs = malloc(sizeof(thread_args_t));
        targs->argv = cleaned_argv;
        targs->is_background = cmd->is_background;

        pid_t return_value_from_u_fg = u_fg(targs);

        int wstatus;
        if (s_waitpid(return_value_from_u_fg, &wstatus, false, false, -1) ==
            -1) {
          u_perror("Failed to waitpid for FG");
        }
      }

      // NICE handled separately
      if (strcmp(input_cmd, "nice") == 0) {
        if (cmd->commands[0][1] == NULL || cmd->commands[0][2] == NULL) {
          int len = snprintf(msg, sizeof(msg), "Incorrect Usage: nice <priority> <command>\n");
          s_write(STDOUT_FILENO, len, msg);
          continue;
        }

        int priority = atoi(cmd->commands[0][1]);
          if (priority > 2  || priority < 0){
            k_print("Error! : Priority must be 0,1,2\n");
            continue;
          }
        const char* command = cmd->commands[0][2];

        void* (*func)(void*) = NULL;
        for (int i = 0; i < number_commands; ++i) {
          if (strcmp(command, command_table[i].name) == 0) {
            func = command_table[i].function;
            break;
          }
        }

        if (func == NULL) {
          int len = snprintf(msg, sizeof(msg), "Unknown command for nice\n");
          s_write(STDOUT_FILENO, len, msg);
          continue;
        }

        thread_func_to_run = func;
        int argc = 0;
        while (cmd->commands[0][2 + argc] != NULL)
          argc++;

        thread_args_t* targs = malloc(sizeof(thread_args_t));
        targs->argv = malloc(sizeof(char*) * (argc + 2));
        targs->argv[0] = strdup(command);
        for (int i = 0; i < argc; i++) {
          targs->argv[i + 1] = strdup(cmd->commands[0][2 + i]);
        }
        targs->argv[argc + 1] = NULL;
        targs->is_background = cmd->is_background;
        // Set the foreground process to the current process
        current_foreground_pid =
            s_spawn(wrapper, targs, stdin_fd, stdout_fd, 2, priority, P_BLOCKED,
                    false, cmd->is_background);
        if (current_foreground_pid == -1) {
          u_perror("s_spawn: failed to fork Nice");
        }
        continue;
      }
      // NORMAL COMMANDS
      for (int i = 0; i < number_commands; ++i) {
        // Check if the command is in the table
        if (strcmp(input_cmd, command_table[i].name) == 0) {
          thread_func_to_run = command_table[i].function;

          thread_args_t* targs = malloc(sizeof(thread_args_t));
          targs->argv = cleaned_argv;
          targs->is_background = cmd->is_background;

          if (command_table[i].is_builtin) {
            command_table[i].function(targs);
          } else {
            proc_fd_ent* file_descriptors = get_file_descriptors();
            if (cmd->stdout_file != NULL) {
              int stdout_file_fd;
              if (cmd->is_file_append) {  // Append mode
                stdout_file_fd = s_open(cmd->stdout_file, F_APPEND);
                if (stdout_file_fd == -1) {
                  u_perror("s_open: Failed");
                }
              } else {  // Write mode
                stdout_file_fd = s_open(cmd->stdout_file, F_WRITE);
                if (stdout_file_fd == -1) {
                  u_perror("s_open: Failed");
                }
              }
              if (stdout_file_fd < 0) {
                int len = snprintf(msg, sizeof(msg), "[s-open] %s failed %s\n", cmd->stdin_file, cmd->commands[0][0]);
                s_write(STDERR_FILENO, len, msg);
              } else {
                // Redirect stdin to stdin_file_fd
                file_descriptors[1] = file_descriptors[stdout_file_fd];
                file_descriptors[1].proc_fd = 1;
              }
            }
            if (cmd->stdin_file != NULL) {
              int stdin_file_fd = s_open(cmd->stdin_file, F_READ);
              if (stdin_file_fd == -1) {
                u_perror("s_open: Failed");
              }
              if (stdin_file_fd < 0) {
                int len = snprintf(msg, sizeof(msg), "[s-open] %s failed %s\n", cmd->stdin_file, cmd->commands[0][0]);
                s_write(STDERR_FILENO, len, msg);

              } else {
                // Redirect stdin to stdin_file_fd
                file_descriptors[0] = file_descriptors[stdin_file_fd];
                file_descriptors[0].proc_fd = 0;
              }
            }
            current_foreground_pid =
                s_spawn(wrapper, targs, 0, 1, 2, 1, P_BLOCKED, false,
                        cmd->is_background);
            if (current_foreground_pid == -1) {
              char buf[256];
              snprintf(buf, sizeof(buf), "s_spawn: failed to fork %s",
                       input_cmd);
              u_perror(buf);
            }
            file_descriptors[0] = stdin_proc_fd;
            file_descriptors[1] = stdout_proc_fd;

            if (!cmd->is_background) {
              // Wait for foreground process to finish
              int wstatus;
              if (s_waitpid(current_foreground_pid, &wstatus, false, false,
                            -1) == -1) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Failed to waitpid for %s ",
                         input_cmd);
                u_perror(buf);
              }
            }
          }
          break;
        }
      }
      // Exec Script Files
      if (!thread_func_to_run && strcmp(input_cmd, "fg") != 0) {
        int perm = s_perm(input_cmd);
        if (perm != PERM_ALL && perm != PERM_EXEC && perm != PERM_READ_EXEC) {
          int len;
          if (perm < 0)
            len = snprintf(msg, sizeof(msg), "SCRIPT FILE NOT FOUND %s\n", input_cmd);
          else
            len = snprintf(msg, sizeof(msg), "EXEC PERMISSION DENIED %s\n", input_cmd);
          s_write(STDOUT_FILENO, len, msg);
          continue;
        }
        int stdout_file_fd = -1;
        int stdin_file_fd = -1;
        proc_fd_ent* file_descriptors = get_file_descriptors();
        if (cmd->stdout_file != NULL) {
          stdout_file_fd = redirect_stdout(cmd, file_descriptors);
          if (stdout_file_fd < 0) {
            continue;
          } else {
            int len = snprintf(msg, sizeof(msg), "stdout redirected to %s\n", cmd->stdout_file);
            s_write(STDOUT_FILENO, len, msg);
          }
        }
        if (cmd->stdin_file != NULL) {
          stdin_file_fd = redirect_stdin(cmd, file_descriptors);
          if (stdin_file_fd < 0) {
            // Close redirected stdout and reset
            file_descriptors[1] = stdout_proc_fd;
            continue;
          } else {
            int len = snprintf(msg, sizeof(msg), "stdin redirected to %s\n", cmd->stdin_file);
            s_write(STDOUT_FILENO, len, msg);
          }
        }
        int script_fd = s_open(input_cmd, F_READ);
        if (script_fd < 0) {
          continue;
        }
        process_script_lines(script_fd);
      }
    }
    s_reap_zombies();  // for reaping background
  }
  s_exit();
  return NULL;
}
