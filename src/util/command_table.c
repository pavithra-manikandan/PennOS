
#include "./command_table.h"
#include "./syscall/sys_call.h"
#include "./userfunctions/stress.h"
#include "./userfunctions/user_functions.h"

/*
 * Command table for the shell.
 * Each entry contains the command name, description, function pointer,
 * and whether it's a built-in command.
 *
 * Note: The function pointers should match the expected signature for
 * user functions.
 * You may need to adjust the function signatures in the user functions
 * to match the expected signature in the command table.
 * The function signature should be:
 * void* function_name(void* arg);
 * where arg is a pointer to the arguments passed to the function.
 */
command_t command_table[] = {
    {"ps", "List all processes.", u_ps, false},
    {"cat", "Concatenate files and print to stdout.", u_cat, false},
    {"sleep", "Sleep for n seconds.", u_sleep, false},
    {"busy", "Busy wait indefinitely.", u_busy, false},
    {"echo", "Echo back input string.", u_echo, false},
    {"ls", "List files.", s_ls, false},
    {"touch", "Create or update files.", s_touch, false},
    {"mv", "Rename a file.", s_mv, false},
    {"cp", "Copy a file.", s_cp, false},
    {"rm", "Remove files.", s_rm, false},
    {"chmod", "Change permissions.", s_chmod, false},
    {"kill", "Send signals to processes.", u_kill, false},
    {"zombify", "Test zombifying.", zombify, false},
    {"orphanify", "Test orphanifying.", orphanify, false},
    {"nice", "Run with a given priority.", u_nice, true},
    {"nice_pid", "Set priority for PID.", u_nice_pid, true},
    {"man", "List all commands.", u_man, true},
    {"bg", "Resume background job.", u_bg, true},
    {"jobs", "List background jobs.", u_jobs, true},
    {"logout", "Exit the shell.", u_logout, true},
    {"edit", "Recursively spawn processes.", u_edit, true},
    {"hang", "Spawn 10 nappers.", hang, true},
    {"nohang", "Spawn 10 nappers non-blocking.", nohang, true},
    {"recur", "Recursively spawn processes.", recur, true},
    {"crash", "Crash the system.", crash, true},
    {"clear", "Clear Screen.", u_clear, true},
    {"wc", "Count the number of lines, words and characters in a file.", u_wc,
     true}};

int number_commands = sizeof(command_table) / sizeof(command_t);
