#include "./pennshell_helper.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "./util/p_errno.h"

#define HISTORY_FILE ".pennsh_history"

char history[HISTORY_LIMIT][128];   // In-memory history buffer
int history_size = 0;               // Number of lines in history
int history_pos = -1;               // Current position in history
int history_loaded = 0;             // Flag to check if history is loaded
pid_t current_foreground_pid = -1;  // Current foreground process ID
struct termios orig_termios;        // Original terminal attributes

// Handles Ctrl+C by terminating the process
void sigint_handler(int sig) {
  if (sig == SIGINT && current_foreground_pid > 1) {
    if (s_kill(current_foreground_pid, P_SIGTERM) == -1) {
      u_perror("s_kill: invalid signal");
    }
  }
}

// Handles Ctrl+Z by stopping the process
void sigstop_handler(int sig) {
  if (sig == SIGTSTP && current_foreground_pid > 1) {
    if (s_kill(current_foreground_pid, P_SIGSTOP) == -1) {  // Stop the process
      u_perror("s_kill: invalid signal");
    }
  }
}

// Handles Ctrl-\ by sending SIGQUIT to foreground
void sigquit_handler(int sig) {
  if (sig == SIGQUIT && current_foreground_pid > 1) {
    if (s_kill(current_foreground_pid, P_SIGQUIT) == -1) {
      u_perror("s_kill: invalid signal");
    }
  }
}

// Registers signal handlers for SIGINT and SIGTSTP
void setup_signals() {
  struct sigaction sig_int;
  sig_int.sa_handler = sigint_handler;
  sig_int.sa_flags = SA_RESTART;
  sigemptyset(&sig_int.sa_mask);
  if (sigaction(SIGINT, &sig_int, NULL) == -1) {
    perror("sigaction (SIGINT) failed");
    exit(EXIT_FAILURE);
  }

  struct sigaction sig_stop;
  sig_stop.sa_handler = sigstop_handler;
  sig_stop.sa_flags = SA_RESTART;
  sigemptyset(&sig_stop.sa_mask);
  if (sigaction(SIGTSTP, &sig_stop, NULL) == -1) {
    perror("sigaction (SIGTSTP) failed");
    exit(EXIT_FAILURE);
  }

  struct sigaction sig_quit;
  sig_quit.sa_handler = sigquit_handler;
  sig_quit.sa_flags = SA_RESTART;
  sigemptyset(&sig_quit.sa_mask);
  if (sigaction(SIGQUIT, &sig_quit, NULL) == -1) {
    perror("sigaction (SIGQUIT) failed");
    exit(EXIT_FAILURE);
  }
}

// Clears the current line and reprints the shell prompt
void clear_line_and_prompt() {
  s_write(1, 5, "\r\x1b[2K");          // Clear line
  s_write(1, strlen(PROMPT), PROMPT);  // Write prompt
}

// Adds a new line to the in-memory history buffer
void add_to_history(const char* line) {
  if (history_size < HISTORY_LIMIT) {
    strcpy(history[history_size++], line);
  } else {
    for (int i = 1; i < HISTORY_LIMIT; ++i) {
      strcpy(history[i - 1], history[i]);
    }
    strcpy(history[HISTORY_LIMIT - 1], line);
  }
}

// Appends a command line to the persistent history file
void save_history_line(const char* line) {
  int fd = open(HISTORY_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd >= 0) {
    s_write(fd, strlen(line), line);
    s_write(fd, 1, "\n");
    close(fd);
  }
}

// Loads command history from the persistent history file
void load_history() {
  if (history_loaded)
    return;
  history_loaded = 1;

  int fd = open(HISTORY_FILE, O_RDONLY);
  if (fd < 0)
    return;

  char buf[128];
  int pos = 0;
  char c;
  while (read(fd, &c, 1) == 1) {
    if (c == '\n') {
      buf[pos] = '\0';
      add_to_history(buf);
      pos = 0;
    } else if (pos < 127) {
      buf[pos++] = c;
    }
  }
  close(fd);
}

// Completes partially typed commands using the command table
void autocomplete(char* buf, int* buf_len, int* cursor_pos) {
  buf[*buf_len] = '\0';
  for (int i = 0; i < number_commands; i++) {
    if (strncmp(buf, command_table[i].name, strlen(buf)) == 0) {
      int remaining = strlen(command_table[i].name) - *buf_len;
      if (remaining > 0) {
        strcpy(buf + *buf_len, command_table[i].name + *buf_len);
        s_write(1, remaining, command_table[i].name + *buf_len);
        *cursor_pos += remaining;
        *buf_len += remaining;
      }
      return;
    }
  }
}

// Reads a line of user input with support for editing and history navigation
int read_input_line(char* buf) {
  int buf_len = 0;
  int cursor_pos = 0;
  static int prompt_displayed = 0;
  history_pos = history_size;
  if (!prompt_displayed) {
    clear_line_and_prompt();  // Show prompt once per line
    prompt_displayed = 1;
  }

  while (1) {
    char c;
    ssize_t r = read(STDIN_FILENO, &c, 1);

    if (r < 0) {
      if (aio_enabled && errno == EAGAIN)
        return 0;  // no input
      continue;    // real error, retry
    }
    if (r == 0)
      return -1;  // EOF

    if (c == 4 && buf_len == 0)
      return -1;  // Ctrl+D
    if (c == 12) {
      s_clear();
      return 0;
    }  // Ctrl+L

    if (c == 1) {  // Ctrl+A
      while (cursor_pos > 0) {
        s_write(1, 3, "\x1b[D");
        cursor_pos--;
      }
      continue;
    }
    if (c == 5) {  // Ctrl+E
      while (cursor_pos < buf_len) {
        s_write(1, 3, "\x1b[C");
        cursor_pos++;
      }
      continue;
    }
    if (c == 11) {  // Ctrl+K
      for (int i = cursor_pos; i < buf_len; i++)
        s_write(1, 1, " ");
      for (int i = cursor_pos; i < buf_len; i++)
        s_write(1, 3, "\x1b[D");
      buf[cursor_pos] = '\0';
      buf_len = cursor_pos;
      continue;
    }
    if (c == 21) {  // Ctrl+U
      while (cursor_pos > 0) {
        s_write(1, 3, "\b \b");
        cursor_pos--;
      }
      memmove(buf, buf + cursor_pos, buf_len - cursor_pos);
      buf_len -= cursor_pos;
      cursor_pos = 0;
      continue;
    }
    if (c == '\t') {  // Tab
      autocomplete(buf, &buf_len, &cursor_pos);
      continue;
    }
    if (c == '\n') {
      buf[buf_len] = '\0';
      s_write(1, 1, "\n");
      if (buf_len > 0) {
        add_to_history(buf);
        save_history_line(buf);
      }
      prompt_displayed = 0;
      return buf_len;
    }
    if (c == 127) {  // Backspace
      if (cursor_pos > 0) {
        memmove(buf + cursor_pos - 1, buf + cursor_pos, buf_len - cursor_pos);
        buf_len--;
        cursor_pos--;
        s_write(1, 1, "\b");
        s_write(1, buf_len - cursor_pos, &buf[cursor_pos]);
        s_write(1, 1, " ");
        for (int i = 0; i <= buf_len - cursor_pos; i++)
          s_write(1, 3, "\x1b[D");
      }
      continue;
    }
    if (c == '\x1b') {  // Arrow keys
      char seq[2];
      if (read(STDIN_FILENO, seq, 2) == 2 && seq[0] == '[') {
        if (seq[1] == 'A') {  // Up
          if (history_pos > 0) {
            history_pos--;
            clear_line_and_prompt();
            strcpy(buf, history[history_pos]);
            buf_len = strlen(buf);
            cursor_pos = buf_len;
            s_write(1, buf_len, buf);
          }
        } else if (seq[1] == 'B') {  // Down
          if (history_pos < history_size - 1) {
            history_pos++;
            clear_line_and_prompt();
            strcpy(buf, history[history_pos]);
            buf_len = strlen(buf);
            cursor_pos = buf_len;
            s_write(1, buf_len, buf);
          } else {
            history_pos = history_size;
            clear_line_and_prompt();
            buf_len = 0;
            cursor_pos = 0;
          }
        } else if (seq[1] == 'C' && cursor_pos < buf_len) {  // Right
          s_write(1, 3, "\x1b[C");
          cursor_pos++;
        } else if (seq[1] == 'D' && cursor_pos > 0) {  // Left
          s_write(1, 3, "\x1b[D");
          cursor_pos--;
        }
      }
      continue;
    }

    if (buf_len < 127) {  // Normal input
      memmove(buf + cursor_pos + 1, buf + cursor_pos, buf_len - cursor_pos);
      buf[cursor_pos] = c;
      buf_len++;
      s_write(1, buf_len - cursor_pos, &buf[cursor_pos]);
      cursor_pos++;
      for (int i = cursor_pos; i < buf_len; i++)
        s_write(1, 3, "\x1b[D");
    }
  }
}

// Restores the terminal to normal (canonical) mode
void disable_raw_mode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// Enables raw terminal mode for live character input
void enable_raw_mode() {
  struct termios raw;
  tcgetattr(STDIN_FILENO, &orig_termios);
  raw = orig_termios;
  raw.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}