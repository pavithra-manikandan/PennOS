
#include "./kernel/kernel.h"
#include "./kernel/kernel_helper.h"
#include "./util/p_errno.h"

int main(int argc, char* argv[]) {
  // Check for correct number of arguments and set aio_enabled to true if async
  if (argc > 2 && strcmp(argv[2], "--aio") == 0) {
    aio_enabled = true;
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
  }
  // Mount PennFAT FS
  if (pmount(argv[1]) == -1) {
    k_print("Failed to mount PennFAT");
  }
  char* log_fname = "log";  // default log file

  if (argc > 2 && strcmp(argv[2], "--aio") != 0) {
    log_fname = argv[2];  // use the provided log file name
  }
  log_init(log_fname);  // Initialize logging
  scheduler_init();     // Initialize the scheduler
  init_kernel();        // Initialize the kernel
  init_shell();         // Initialize the shell
  thread_args_t shell_args = {.argv = (char*[]){"shell", NULL},
                              .is_background = false};

  // Spawn the shell process
  if (s_spawn(penn_shell, &shell_args, 0, 1, 1, 0, P_BLOCKED, true, false) ==
      -1) {
    k_print("s_spawn: Failed to fork Penn Shell");
  }
  // Wait for the shell process to finish
  spthread_join(s_get_pcb_with_given_pid(1)->thread, NULL);
  return 0;
}