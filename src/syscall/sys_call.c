#include "./sys_call.h"
#include <stdarg.h>
#include <termios.h>  //For extra credit
#include <unistd.h>   // For extra credit
#include "./shell/pennshell_helper.h"
#include "./util/p_errno.h"

/// Spawn a new process
pid_t s_spawn(void* (*func)(void*),
              thread_args_t* t_args,
              int fd0,
              int fd1,
              int parent_id,
              int priority,
              int status,
              bool is_init,
              bool is_background) {
  char** argv = t_args->argv;
  pid_t child_pid =
      k_fork(func, argv, fd0, fd1, parent_id, priority, status, is_init,
             is_background);  // create a new process Child
  if (child_pid == -1) {
    P_ERRNO = P_EFORK;
  }
  return child_pid;
}

// Wait for a child process to finish
pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang, bool is_init, int ppid) {
  return k_waitpid(pid, wstatus, nohang, is_init, -1);
}

// Send a signal to a process
int s_kill(pid_t pid, int signal) {
  return k_kill(pid, signal);
}

// Unconditionally exit the calling process
void s_exit() {
  k_exit();
}

// Change the priority of a process
int s_nice(pid_t pid, int priority) {
  if (priority < 0 || priority > 2) {  // Prioirty is 0,1,2
    k_print("Priority must be 0 , 1 or 2.\n");
    P_ERRNO = P_EINVAL_NICE;
    return -1;
  }
  return k_nice(pid, priority);
}

// Sleep for a specified number of ticks
void s_sleep(unsigned int ticks) {
  k_sleep(ticks);
  return;
}

// Print the process list
void s_ps() {
  k_ps();
}

// Bring a background job to the foreground
pid_t s_fg(int job_id) {
  return k_fg(job_id);
}

// Resume a stopped job in the background
int s_bg(int job_id) {
  return k_bg(job_id);
}
// Print the job list
void s_jobs() {
  k_jobs();
}

int is_posix(const char* fname) {
  for (int i = 0; i < strlen(fname); i++) {
    if (fname[i] == 45 || fname[i] == 46 ||
        (fname[i] >= 48 && fname[i] <= 57) ||
        (fname[i] >= 65 && fname[i] <= 90) || fname[i] == 95 ||
        (fname[i] >= 97 && fname[i] <= 122)) {
      continue;
    } else {
      return 0;
    }
  }
  return 1;
}

// Get the file descriptor table of the current thread
proc_fd_ent* get_file_descriptors() {
  spthread_t self;
  spthread_self(&self);
  pcb_t* pcb = find_parent_with_current_thread();

  if (!pcb) {
    k_print("[s_write] get_pcb_from_thread() returned NULL!\n");
  }
  if (!pcb->file_descriptors) {
    k_print("[s_write] pcb->file_descriptors is NULL!\n");
  }

  return pcb->file_descriptors;
}

// Validate fd
int is_valid_fd(int fd) {
  return fd >= 0 && fd < MAX_OPEN_FILES;
}

int s_open(const char* fname, int mode) {
  // Validate fname and mode
  if (mode < 0 || mode > 2) {
    P_ERRNO = INVALID_MODE;
    return -1;
  }
  if (!(is_posix(fname))) {
    P_ERRNO = FILENAME_INVALID;
    return -1;
  }
  proc_fd_ent* fd_table = get_file_descriptors();
  int global_fd = k_open(fname, mode);
  if (global_fd < 0) {
    P_ERRNO = FD_INVALID;
    return -1;
  }
  int fd = -1;
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    int pcb_fd = fd_table[i].proc_fd;
    if (pcb_fd < 0) {
      fd = i;
      break;
    }
  }
  if (fd < 0) {
    k_close(global_fd);
    P_ERRNO = TOO_MANY_OPEN_FILES;
    return -1;
  }
  proc_fd_ent fd_ent;
  fd_ent = (proc_fd_ent){.proc_fd = fd,
                         .mode = mode,
                         .offset = (mode == F_APPEND) ? k_file_size(fname) : 0,
                         .global_fd = global_fd};
  fd_table[fd] = fd_ent;
  return fd;
}

int s_close(int fd) {
  if (!(is_valid_fd(fd))) {
    P_ERRNO = FD_INVALID;
    return -1;
  }
  proc_fd_ent* fd_table = get_file_descriptors();
  if (fd_table[fd].proc_fd == -1) {
    P_ERRNO = FD_INVALID;
    return -1;
  }
  k_close(fd_table[fd].global_fd);
  fd_table[fd].proc_fd = -1;
  return 0;
}

int s_write(int fd, int n, const char* str) {
  proc_fd_ent* fd_table = get_file_descriptors();
  if (!fd_table) {
    P_ERRNO = FD_TABLE_NULL;
    return -1;
  }

  if (!(is_valid_fd(fd)) || fd_table[fd].proc_fd == -1) {
    P_ERRNO = FD_INVALID;
    return -1;
  }

  if (n < 0) {
    P_ERRNO = P_EINVAL;
    return -1;
  }

  if (str == NULL) {
    P_ERRNO = P_EINVAL;
    return -1;
  }

  if (fd_table[fd].mode != F_WRITE && fd_table[fd].mode != F_APPEND) {
    P_ERRNO = INVALID_MODE;
    return -1;
  }
  int global_fd = fd_table[fd].global_fd;
  k_lseek(global_fd, fd_table[fd].offset, F_SEEK_SET);
  int bytes_written = k_write(global_fd, str, n);
  if (bytes_written < 0) {
    P_ERRNO = FD_INVALID;
    return -1;
  }

  fd_table[fd].offset += bytes_written;
  return bytes_written;
}

int s_read(int fd, int n, char* buf) {
  proc_fd_ent* fd_table = get_file_descriptors();
  if (!(is_valid_fd(fd)) || fd_table[fd].proc_fd == -1) {
    P_ERRNO = FD_INVALID;
    return -1;
  }
  if (n < 0) {
    P_ERRNO = P_EINVAL;
    return -1;
  }
  int global_fd = fd_table[fd].global_fd;
  k_lseek(global_fd, fd_table[fd].offset, F_SEEK_SET);
  int bytes_read = k_read(global_fd, n, buf);
  if (bytes_read < 0) {
    P_ERRNO = FD_INVALID;
    return -1;
  }
  fd_table[fd].offset += bytes_read;
  return bytes_read;
}

int s_unlink(const char* fname) {
  if (!(is_posix(fname))) {
    k_print("DEBUG[s_unlink]: invalid filename %s\n", fname);
    return FILENAME_INVALID;
  }
  int unlink_val = k_unlink(fname);
  if (unlink_val == FILE_NOT_FOUND) {
    k_print("DEBUG[s_unlink]: file %s not found\n", fname);
    return unlink_val;
  }
  if (unlink_val == FILE_IN_USE) {
    k_print("DEBUG[s_unlink]: file %s in use\n", fname);
    return unlink_val;
  }
  return unlink_val;
}

int s_lseek(int fd, int offset, int whence) {
  proc_fd_ent* fd_table = get_file_descriptors();

  // Validate file descriptor
  if (!is_valid_fd(fd) || fd_table[fd].proc_fd == -1) {
    k_print("DEBUG[s_lseek]: invalid fd %d\n", fd);
    return FD_INVALID;
  }

  // Validate whence
  if (whence != F_SEEK_SET && whence != F_SEEK_CUR && whence != F_SEEK_END) {
    k_print("DEBUG[s_lseek]: invalid whence %d\n", whence);
    return INVALID_WHENCE;
  }

  int global_fd = fd_table[fd].global_fd;

  // Call kernel-level seek
  int result = k_lseek(global_fd, offset, whence);
  if (result < 0) {
    k_print("DEBUG[s_lseek]: kernel lseek failed\n");
    return result;
  }

  // Update local offset
  fd_table[fd].offset = result;
  return result;
}

int s_perm(const char* fname) {
  if (!is_posix(fname)) {
    return FILENAME_INVALID;
  } else {
    return k_perm(fname);
  }
}

void* s_ls(void* arg) {
  thread_args_t* targs = (thread_args_t*)arg;

  const char* filename = NULL;
  if (targs && targs->argv && targs->argv[1]) {
    filename = targs->argv[1];
  }

  ls(filename);
  return NULL;
}

// Helper function to create a new file
void* s_touch(void* arg) {
  char** argv = (char**)arg;
  if (!argv) {
    k_print("touch: invalid args\n");
    return NULL;
  }
  int argc = 0;

  while (argv[argc] != NULL) {
    argc++;
  }
  return (void*)(long)ptouch(argc, argv);
}

// Helper function to remove a file
void* s_rm(void* arg) {
  char** argv = (char**)arg;

  if (!argv || !argv[1]) {
    k_print("Usage: rm <file>\n");
    return NULL;
  }
  int i = 1;
  while (argv[i]) {
    k_print("%s this is argv %d\n", argv[i], i);
    rm(argv[i]);
    i++;
  }
  return NULL;
}

//  Helper function to rename a file
void* s_mv(void* arg) {
  char** argv = (char**)arg;

  if (!argv || !argv[1] || !argv[2]) {
    k_print("Usage: mv <source> <destination>\n");
    return NULL;
  }
  mv(argv[1], argv[2]);
  return NULL;
}

// Helper function to copy a file
void* s_cp(void* arg) {
  char** argv = (char**)arg;

  // Count argc
  int argc = 0;
  while (argv && argv[argc])
    argc++;

  if (argc < 3) {
    k_print("Usage: cp <src1> [src2 ...] <dest>\n");
    return NULL;
  }
  cp(argc, argv);
  return NULL;
}

// Helper function to concatenate files
void* s_cat(void* arg) {
  char** argv = (char**)arg;

  // Count argc
  int argc = 0;
  while (argv && argv[argc]) {
    argc++;
  }

  (void)k_cat(argc, argv);
  return NULL;
}

void* s_chmod(void* arg) {
  char** argv = (char**)arg;

  if (!argv || !argv[1] || !argv[2]) {
    k_print("Usage: chmod <filename> <perm>\n");
    return NULL;
  }
  const char* filename = argv[2];
  int result = INVALID_MODE;
  if (argv[1][0] == '-') {
    for (int i = 1; argv[1][i]; i++) {
      if (argv[1][i] == 'r') {
        result = chmod(filename, -PERM_READ);
      } else if (argv[1][i] == 'w') {
        result = chmod(filename, -PERM_WRITE);
      } else if (argv[1][i] == 'x') {
        result = chmod(filename, -PERM_EXEC);
      }
    }
  } else if (argv[1][0] == '+') {
    for (int i = 1; argv[1][i]; i++) {
      if (argv[1][i] == 'r') {
        result = chmod(filename, PERM_READ);
      } else if (argv[1][i] == 'w') {
        result = chmod(filename, PERM_WRITE);
      } else if (argv[1][i] == 'x') {
        result = chmod(filename, PERM_EXEC);
      }
    }
  }
  if (result < 0) {
    k_print("Chmod failed with error code %d\n", result);
  }

  return NULL;
}

void* s_echo(void* arg) {
  char** t_args = (char**)arg;
  char** argv = t_args;

  char buffer[MAX_BUFFER_SIZE];  // max size of echo output
  int len = 0;
  // Build output string
  for (int i = 1; argv[i]; ++i) {
    int word_len = strlen(argv[i]);
    if (len + word_len + 2 >= sizeof(buffer)) {
      panic("echo: output too long");
      return NULL;
    }

    memcpy(buffer + len, argv[i], word_len);
    len += word_len;

    if (argv[i + 1]) {
      buffer[len++] = ' ';
    }
  }

  buffer[len++] = '\n';

  // Write once
  if (s_write(1, len, buffer) == -1) {
    u_perror("s_write: Failed");
  }
  proc_fd_ent* file_table = get_file_descriptors();
  if (file_table[STDIN_FILENO].global_fd != 0) {
    s_close(STDIN_FILENO);
    file_table[STDIN_FILENO] = (proc_fd_ent){
        .proc_fd = 0, .mode = F_READ, .offset = 0, .global_fd = 0};
  }
  if (file_table[STDOUT_FILENO].global_fd != 1) {
    s_close(STDOUT_FILENO);
    file_table[STDOUT_FILENO] = (proc_fd_ent){
        .proc_fd = 1, .mode = F_WRITE, .offset = 0, .global_fd = 1};
  }
  return NULL;
}

// Helper function to reap zombie processes
void s_reap_zombies() {
  k_reap_zombies();
}

pcb_t* s_get_pcb_with_given_pid(pid_t pid) {
  return k_get_pcb_with_given_pid(pid);
}

/************************************
 *       EXTRA CREDIT FUNCTIONS      *
 *************************************
 */

void* s_clear() {
  k_clear();
  return NULL;
}

// Helper function to write formatted output using s_write
void s_print(const char* fmt, ...) {
  char buf[512];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (len > 0) {
    s_write(1, len, buf);
  }
}

// Redraw current prompt and buffer line after editing
void redraw_current_line(int current_line, char* line, int cursor_pos) {
  // Move to beginning of line
  s_write(1, 1, "\r");

  // Clear line (overwrite with spaces)
  char clear_buf[512];
  int prompt_len =
      snprintf(clear_buf, sizeof(clear_buf), "[%d]> ", current_line + 1);
  int total_len = prompt_len + strlen(line) + 10;
  memset(clear_buf + prompt_len, ' ', total_len - prompt_len);
  clear_buf[total_len] = '\0';
  s_write(1, total_len, clear_buf);

  // Move back to beginning
  s_write(1, 1, "\r");

  // Print prompt and buffer
  s_print("[%d]> %s", current_line + 1, line);

  // Move cursor to correct position
  int move_left = strlen(line) - cursor_pos;
  if (move_left > 0) {
    char seq[16];
    snprintf(seq, sizeof(seq), "\x1b[%dD", move_left);
    s_write(1, strlen(seq), seq);
  }
}

void* s_edit(void* arg) {
  struct termios orig_termios, raw;
  tcgetattr(STDIN_FILENO, &orig_termios);
  raw = orig_termios;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  thread_args_t* t_args = (thread_args_t*)arg;
  char** argv = t_args->argv;

  if (!argv[1]) {
    s_print("Usage: edit <filename>\n");
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    return NULL;
  }

  char* filename = argv[1];
  char buffer[100][256] = {0};
  int line_count = 0;
  int current_line = 0;
  int dirty = 0;

  // Load file
  int fd = k_open(filename, F_READ);
  if (fd >= 0) {
    char bigbuf[4096] = {0};
    ssize_t bytes_read = k_read(fd, sizeof(bigbuf) - 1, bigbuf);
    if (bytes_read > 0) {
      bigbuf[bytes_read] = '\0';
      char* line = strtok(bigbuf, "\n");
      while (line && line_count < 100) {
        strncpy(buffer[line_count], line, sizeof(buffer[0]) - 1);
        line = strtok(NULL, "\n");
        line_count++;
      }
    }
    k_close(fd);
  }

  s_write(1, 1, "\n");
  s_print("=== PennOS Text Editor ===\n");
  s_print("Editing: %s\n", filename);
  s_print("Commands:\n");
  s_print("  :w - Save    :q - Quit    :wq - Save & Quit\n");
  s_print("  :up - Move cursor up    :down - Move cursor down\n");
  s_print("  :d - Delete current line    :n - Insert new line\n\n");

  int running = 1;

  while (running) {
  redraw:
    s_write(1, 1, "\n");
    for (int i = 0; i < line_count; i++) {
      s_print("%s%3d: %s%s\n", (i == current_line) ? ">" : " ", i + 1,
              buffer[i], (i == current_line) ? " <" : "");
    }
    if (current_line == line_count) {
      s_print("> %3d:  <\n", current_line + 1);
      buffer[current_line][0] = '\0';
    }

    s_print("\n[%d]> %s", current_line + 1, buffer[current_line]);
    fflush(stdout);

    int cursor_pos = strlen(buffer[current_line]);
    char ch;

    while (read(STDIN_FILENO, &ch, 1) == 1) {
      if (ch == '\n') {
        s_write(1, 1, "\n");
        break;
      } else if (ch == 127 || ch == '\b') {  // Backspace
        if (cursor_pos > 0) {
          memmove(&buffer[current_line][cursor_pos - 1],
                  &buffer[current_line][cursor_pos],
                  strlen(&buffer[current_line][cursor_pos]) + 1);
          cursor_pos--;
          dirty = 1;
          redraw_current_line(current_line, buffer[current_line], cursor_pos);
        }
      } else if (ch == '\x1b') {  // Escape sequence (arrow keys)
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) == 1 &&
            read(STDIN_FILENO, &seq[1], 1) == 1) {
          if (seq[0] == '[') {
            switch (seq[1]) {
              case 'A':  // Up
                if (current_line > 0) {
                  current_line--;
                  goto redraw;
                }
                break;
              case 'B':  // Down
                if (current_line < line_count) {
                  current_line++;
                  goto redraw;
                }
                break;
              case 'C':  // Right
                if (cursor_pos < strlen(buffer[current_line])) {
                  cursor_pos++;
                  redraw_current_line(current_line, buffer[current_line],
                                      cursor_pos);
                }
                break;
              case 'D':  // Left
                if (cursor_pos > 0) {
                  cursor_pos--;
                  redraw_current_line(current_line, buffer[current_line],
                                      cursor_pos);
                }
                break;
            }
          }
        }
      } else if (ch == ':') {
        // Command mode
        char command[32] = {0};
        int i = 0;
        command[i++] = ch;
        s_write(1, 1, &ch);
        while (i < sizeof(command) - 1 && read(STDIN_FILENO, &ch, 1) == 1 &&
               ch != '\n') {
          command[i++] = ch;
          s_write(1, 1, &ch);
        }
        command[i] = '\0';
        s_write(1, 1, "\n");

        if (strcmp(command, ":w") == 0 || strcmp(command, ":wq") == 0) {
          fd = k_open(filename, F_WRITE);
          if (fd >= 0) {
            for (int i = 0; i < line_count; i++) {
              k_write(fd, buffer[i], strlen(buffer[i]));
              k_write(fd, "\n", 1);
            }
            k_close(fd);
            dirty = 0;
            s_print("Saved %d lines.\n", line_count);
          } else {
            s_print("Error opening file for writing.\n");
          }
          if (strcmp(command, ":wq") == 0) {
            running = 0;
            break;
          }
        } else if (strcmp(command, ":q") == 0) {
          if (dirty) {
            s_print("Warning: Unsaved changes. Use :wq to save and quit.\n");

          } else {
            running = 0;
            break;
          }
        } else if (strcmp(command, ":up") == 0) {
          if (current_line > 0)
            current_line--;
        } else if (strcmp(command, ":down") == 0) {
          if (current_line < line_count)
            current_line++;
        } else if (strcmp(command, ":d") == 0) {
          if (current_line < line_count) {
            for (int i = current_line; i < line_count - 1; i++) {
              strcpy(buffer[i], buffer[i + 1]);
            }
            buffer[line_count - 1][0] = '\0';
            line_count--;
            dirty = 1;
            if (current_line >= line_count)
              current_line = line_count > 0 ? line_count - 1 : 0;
          }
        } else if (strcmp(command, ":n") == 0) {
          if (line_count < 100) {
            for (int i = line_count; i > current_line; i--) {
              strcpy(buffer[i], buffer[i - 1]);
            }
            buffer[current_line][0] = '\0';
            line_count++;
            dirty = 1;
          } else {
            s_print("Buffer full. Cannot insert new line.\n");
          }
        }
        goto redraw;
      } else {
        // Insert character at cursor_pos
        int len = strlen(buffer[current_line]);
        if (len < sizeof(buffer[current_line]) - 1) {
          memmove(&buffer[current_line][cursor_pos + 1],
                  &buffer[current_line][cursor_pos], len - cursor_pos + 1);
          buffer[current_line][cursor_pos] = ch;
          cursor_pos++;
          dirty = 1;
          redraw_current_line(current_line, buffer[current_line], cursor_pos);
        }
      }
    }

    if (current_line == line_count) {
      line_count++;
    }
    current_line++;
  }

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  return NULL;
}

// Helper function to count lines, words, and characters in a file
void* s_wc(void* arg) {
  char** argv = (char**)arg;
  char msg[PRINT_BUFFER_SIZE];
  if (!argv || !argv[1]) {
    int len = snprintf(msg, sizeof(msg), "Usage: wc <filename>\n");
    s_write(STDOUT_FILENO, len, msg);
    return NULL;
  }

  int lines = 0, words = 0, chars = 0;
  int result = k_wc(argv[1], &lines, &words, &chars);
  if (result < 0) {
    int len = snprintf(msg, sizeof(msg), "wc: failed to read %s\n", argv[1]);
    s_write(STDOUT_FILENO, len, msg);
    return NULL;
  }

  int len =
      snprintf(msg, sizeof(msg), "%d %d %d %s\n", lines, words, chars, argv[1]);
  s_write(STDOUT_FILENO, len, msg);
  return NULL;
}

void s_printer(const char* fmt, ...) {
  k_print(fmt);
}
