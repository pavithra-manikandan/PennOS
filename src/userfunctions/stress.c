#include "stress.h"

#include <signal.h>  // for kill(SIGKILL) to mimic crashing for one of the tests
#include <stdbool.h>
#include <stdio.h>
#include <stdio.h>   // just for snprintf
#include <stdlib.h>  // for malloc
#include <string.h>  // for memcpy and strlen
#include <time.h>    // for time()
#include <unistd.h>

/******************************************************************************
 *                                                                            *
 * Replace syscalls.h with your own header file(s) for s_spawn and s_waitpid. *
 *                                                                            *
 ******************************************************************************/

// #include "syscalls.h"

#include "../syscall/sys_call.h"

// You can tweak the function signature to make it work.
static void* nap(void* arg) {
  s_sleep(1);  // sleep for 1 tick
  s_exit();
  return NULL;
}

/*
 * The function below spawns 10 nappers named child_0 through child_9 and
 waits
 * on them. The wait is non-blocking if nohang is true, or blocking
 otherwise.
 *
 * You can tweak the function signature to make it work.
 */

static void* spawn(bool nohang) {
  //  for hang nohang= false
  static char names[10][8];  
  char* argvs[10][2];      

  int pid = 0;

  // First, prepare all the names and argv pointers
  for (int i = 0; i < 10; i++) {
    snprintf(names[i], sizeof(names[i]), "child_%d",
             i);             // Write "child_0", ..., "child_9"
    argvs[i][0] = names[i];  // argv[0] = name
    argvs[i][1] = NULL;      // argv[1] = NULL
  }

  // Spawn 10 nappers named child_0 through child_9.
  for (int i = 0; i < 10; i++) {
    thread_args_t shell_args = {
        .argv = argvs[i],  // Each child gets its own argv array
        .is_background = false};

    // s_spawn for this to work.
    int fd0 = 0;
    int fd1 = 1;
    const int id =
        s_spawn(nap, &shell_args, fd0, fd1, 2, 1, P_BLOCKED, false, false);

    if (i == 0)
      pid = id;

    // may need to change the arg of the function to make it work
    // that is ok
    s_write(STDERR_FILENO, strlen(argvs[i][0]), argvs[i][0]);
    s_write(STDERR_FILENO, strlen(" was spawned\n"), " was spawned\n");

  
  }
  // Wait on all children.
  while (1) {
    const int cpid = s_waitpid(-1, NULL, nohang, false, -1);
    // polling if nonblocking wait and no waitable children yet
    if (nohang && cpid == 0) {
      s_sleep(9);  // 9 ticks
      continue;
    }
    if (cpid < 0)  // no more waitable children (if block-waiting) or error
      break;


    char child_num_str[4];
    snprintf(child_num_str, 4, "%d", cpid - pid);


    s_write(STDERR_FILENO, strlen("child_ "), "child_");
    s_write(STDERR_FILENO, strlen(child_num_str), child_num_str);
    s_write(STDERR_FILENO, strlen(" was reaped\n"), " was reaped\n");


  }

  return NULL;
}

/*
 * The function below recursively spawns itself 26 times and names the
 spawned
 * processes Gen_A through Gen_Z. Each process is block-waited by its parent.
 *
 * You can tweak the function signature to make it work.
 */

static void* spawn_r(void* arg) {
  static int i = 0;

  int pid = 0;
  char name[] = "Gen_A";
  char* argv[] = {name, NULL};

  if (i < 26) {
    argv[0][sizeof name - 2] = 'A' + i++;

    // may need to change the arg of the function to make it work
    // that is ok
    int fd0 = 0;
    int fd1 = 1;
    thread_args_t shell_args = {
        .argv = argv,           // Arguments to pass
        .is_background = false  // Use the background flag
    };
    pid =
        s_spawn(spawn_r, &shell_args, fd0, fd1, 2, 1, P_BLOCKED, false, false);
    // s_spawn(spawn_r, &shell_args, fd0, fd1, 2, 1, P_BLOCKED, false, false);


    s_write(STDERR_FILENO, strlen(argv[0]), argv[0]);
    s_write(STDERR_FILENO, strlen(" was spawned\n"), " was spawned\n");


    s_sleep(1);  // 1 tick
  }

  if (pid > 0 && pid == s_waitpid(pid, NULL, false, false, -1)) {
    s_write(STDERR_FILENO, strlen(argv[0]), argv[0]);
    s_write(STDERR_FILENO, strlen(" was reaped\n"), " was reaped\n");
    // dprintf(STDERR_FILENO, "%s was reaped\n", *armakegv);
  }
  s_exit();
  return NULL;
}

static char* gen_pattern_str() {
  size_t len = 5480;

  char pattern[9];
  pattern[8] = '\0';

  srand(time(NULL));

  for (size_t i = 0; i < 8; i++) {
    // random ascii printable character
    pattern[i] = (char)((rand() % 95) + 32);
  }

  char* str = malloc((len + 1) * sizeof(char));

  str[5480] = '\0';

  for (size_t i = 0; i < 5480; i += 8) {
    memcpy(&(str[i]), pattern, 8 * sizeof(char));
  }

  return str;
}

// you can change calls to s_write, s_unlink and s_open to match your
// implementation of these functions if needed.
static void crash_main() {
  const char* fname = "CRASHING.txt";
  s_unlink(fname);

  int fd = s_open(fname, F_WRITE);
  printf("%d this is the fd s_open returns\n", fd);

  char* str = gen_pattern_str();

  char* msg =
      "writing a string that consists of the following pattern 685 times to "
      "CRASHING.txt: ";
  s_write(STDERR_FILENO, strlen(msg), msg);

  s_write(STDERR_FILENO, 8, str);

  s_write(STDERR_FILENO, 1, "\n");

  // write the str to the file
  s_write(fd, 5480, str);

  msg = "crashing pennos. Our write should be safe in the file system.";
  s_write(STDERR_FILENO, strlen(msg), msg);

  msg = "We should see this file and this message in a hexdump of the fs\n";
  s_write(STDERR_FILENO, strlen(msg), msg);

  // Yes kill and signals are banned in PennOS.
  // The line below is an exception to mimic having pennos "crash"
  kill(0, SIGKILL);

  msg = "ERROR: PENNOS WAS SUPPOSED TO CRASH\n";
  s_write(STDERR_FILENO, strlen(msg), msg);
}

/******************************************************************************
 * *
 * Add commands hang, nohang, recur, and crash to the shell as built-in *
 * subroutines which call the following functions, respectively. *
 * *
 * you can change these function signautures as needed to make it work *
 * *
 ******************************************************************************/

void* hang(void* arg) {
  spawn(false);
  return NULL;
}

void* nohang(void* arg) {
  spawn(true);
  return NULL;
}

void* recur(void* arg) {
  spawn_r(NULL);
  return NULL;
}

void* crash(void* arg) {
  // This one only works on a file system big enough to hold 5480 bytes
  crash_main();
  return NULL;
}
