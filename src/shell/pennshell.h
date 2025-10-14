#include "./kernel/kernel.h"
#include "./util/parser.h"

#ifndef PSHELL_H_
#define PSHELL_H_
extern bool aio_enabled;
char* my_strdup(char** args);

/**
 * @brief Initialize the shell.
 * This function sets up the shell environment, including signal handlers and
 * other necessary components.
 */
void init_shell();

/**
 * @brief Main shell loop.
 * This function runs the main loop of the shell, handling user input and
 * executing commands.
 */
void* penn_shell(void* arg);


char* preprocess_redirection(const char* input);

void execute_script_command(struct parsed_command *script_cmd);

void process_script_lines(int script_fd);

void reset_redirections();

#endif  // PSHELL_H_
