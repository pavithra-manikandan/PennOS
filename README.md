# PennOS Project Overview
PennOS is a pedagogical operating system developed for educational purposes, providing an in-depth understanding of operating system concepts. It covers key functionalities including process management, job control, file systems, threading, and extensive system call implementations.

## Authors
- **Shravani Chavan** (Pennkey: schavan)
- **Pavithra Manikandan** (Pennkey: pavi20)
- **Rashi Agrawal** (Pennkey: agrras)
- **Khye Facey-Marshall** (Pennkey: khyefac)

## Submitted Source Files
- kernel
    -`kernel_helper.c`
    -`kernel_helper.h`
    - `kernel.c`
    - `kernel.h`
    - `kfat_helper.c`
    - `kfat_helper.h`
- pennfat
    - `pennfat_help.c`
    - `pennfat_help.h`
    - `pennfat.c`
    - `pennfat.h`
-scheduler
    - `log.c`
    - `log.h`
    - `scheduler.c`
    - `scheduler.h`
    - `scheduler_helper.c`
    - `scheduler_helper.h`
-shell
    - `pennshell.c`
    - `pennshell.h`
    - `pennshell_helper.c`
    - `pennshell_helper.h`
-syscall
    - `sys_call.c`
    - `sys_call.h`
-userfunctions
    - `stress.c`
    - `stress.h`
    - `user-functions.c`
    - `user-functions.h`
-util
    - `command_table.c`
    - `command_table.h`
    - `p_errno.h`
    - `p_errno.c`
    - `panic.c`
    - `panic.h`
    - `parser.c`
    - `parser.h`
    - `spthread.c`
    - `spthread.h`
    - `thread_args.h`
-vec
    - `Vec.c`
    - `Vec.h`
- `pcb.h`
- `pennos.c`
-tests
    - `sched-demo.c`

- `Makefile`

## Overview of Work Accomplished
This project implements a basic UNIX-like operating system. The operating system is designed around subsystems including:
- **Priority round-robin scheduler**
- **FAT filesystem**
- **Interactive user shell**

The operating system also maintains sufficient separation between **user mode** and **kernel mode** functions.

## Compilation Instructions
Change directory to penn-os, and use the "make" command.

## Description of Code and Code Layout
- **kernel**

    **kernel_helper.c/h**: Assists with kernel-specific helper functions for process management and internal kernel operations.

    **kernel.c/h:** Core kernel interface for managing process lifecycle, file I/O, job control, and thread-level operations in a UNIX-like operating system. Defines and implements the API for kernel-level management of user processes, thread execution, job control, interactions with the filesystem.
    It works in tandem with other OS subsystems such as the scheduler, PCB manager, and file system (PennFAT). Supports process creation, forking, and cleanup job control (background/foreground jobs), signal-based process termination and suspension file operations such as open, read, write, and seek scheduling and priority adjustments basic waitpid/zombie handling for parent-child processes.

    **kfat_helper.c/h**: Provides helper functions for kernel-level interactions with the FAT filesystem.

- **pennfat**

    **pennfat_help.c/h**: Defines filesystem data structures, helper methods, and shell-level filesystem commands. Contains definition for filesystem-related structs and  implementation of filesystem helper functions. Also contains. implementation of standalone PennFAT shell commands `ptouch`, `mv`, `rm`, `cat`, `cp`, `chmod` and `ls`. Contains functions to `mkfs`, `pmount` and `punmount` to make, mount and unmount FAT filesystems. Also contains a `main()` function to parse command-line arguments and call appropriate functions.

    **pennfat.c/h**: Manages FAT filesystem operations such as creating (mkfs), mounting (pmount), and unmounting (punmount).

- **scheduler**

    **log.c/h**: Records critical events and scheduler actions for debugging purposes.

    **scheduler.c/h**: Implements the round-robin priority scheduler logic. Round robin priority queue scheduler with a fixed quantum of 100 ms. Implements priority levels 0, 1 and 2, where level 1 is scheduled 1.5 times more htan level 2, level 0 is scheduled 1.5 times more than level 1. SIGALRM is triggered every time quantum to run the scheduler. 

    **scheduler_helper.c/h**: Contains auxiliary methods to support scheduler functionality.

- **pennshell**

    **pennshell.c/h**: Provides an interface between users and PennOS protocols. `pennshell` is initialized by the `main()` function of `pennos.c`. This sets a signal handler for SIGINT to deliver a P_SIGTERM to the foreground process of `pennos`. It also implements a read loop to parse user input, initialize the scheduler and call the appropriate user functions. 

    **pennshell_helper.c/h**: Includes supplementary methods assisting the shell with parsing, signal handling, and user interaction.

- **syscall**

    **sys_call.c/h**: Defines interfaces for system call interactions between user-level applications and the kernel.

- **userfunctions**

    **stress.c/h**: Implements commands used for stress testing and validating OS stability.

    **user-functions.c/h**: Provides implementations of common user-space commands and utilities.

- **util**

    **command_table.c/h**: Maintains a structure mapping commands to their handlers.

    **p_errno.c/h**: Manages error codes and reporting mechanisms within PennOS.

    **panic.c/h:** Handles system panic scenarios, providing appropriate cleanup and debugging information.

    **parser.c/h**: Parses user inputs into structured commands and arguments.

    **spthread.c/h**: Implements cooperative threading support.

    **thread_args.h**: Defines structures for thread arguments management.

- **vec**

    **Vec.c/h:** Implements dynamic arrays to support various internal OS operations.

- **Other**
    **pcb.h**: This header defines the data structures and constants used for managing processes.  The `pcb_t` structure represents the state and metadata for each process, including process IDs, parent/child relationships, file descriptors, and status flags.

    **pennos.c**: Entry point of PennOS, initializes kernel subsystems, and starts the interactive shell. Contains a `main()` function which initializes the kernel, the interactive shell, the logger and optionally mounts the filesystem specified in command-line arguments.

## Extra Credit

- **kernel**
`Noncanonical Input Mode Shell`

We implemented an advanced shell input mode using termios(3) to switch the terminal to noncanonical (raw) mode. This enables real-time character processing and replicates several features from GNU Bash:

Arrow Key Support
- Left and Right arrows move the cursor within the line.
- Up and Down arrows navigate through in-memory and persistent command history.
- Control Keys
    - Ctrl-D: Signals EOF if the line is empty.
    - Ctrl-C: Sends a P_SIGTERM to the foreground process.
    - Ctrl-Z: Sends a P_SIGSTOP to the foreground process.
    - Ctrl-\: Handles Ctrl-\ by sending SIGQUIT to foreground
    - Ctrl-A: Moves the cursor to the beginning of the line.
    - Ctrl-E: Moves the cursor to the end of the line.
    - Ctrl-K: Deletes from cursor to end of line.
    - Ctrl-U: Deletes from beginning of line to cursor.

- Tab Autocompletion
    When Tab is pressed, the shell uses a prefix match from the command table to auto-complete the current command.

- Delete Key
    The delete/backspace key removes characters before the cursor, shifting the buffer accordingly.

- Command History
    Shell maintains an in-memory history buffer and persists the history to .pennsh_history.On startup, previous session history is loaded and accessible via up/down arrows.

These enhancements offer a fully interactive and responsive command-line experience in PennOS, closely emulating Bash behavior.

`Asynchronous read from terminal`
    To improve CPU utilization, we implemented asynchronous input handling for the shell using fcntl.h. When enabled, terminal reads are non-blocking and checked for availability, enabling processes like busy to scale up from ~40% to ~100% CPU usage in background mode.
    
    Asynchronous input mode is activated by launching the OS with the --aio flag
    The terminal file descriptor is set to non-blocking mode using fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK).

    Read loops in the shell return immediately if no input is available (EAGAIN), avoiding unnecessary blocking and improving responsiveness and system concurrency.

    This enhancement helps demonstrate real-world asynchronous I/O handling and improves background task performance in PennOS.


- **PennFAT**
`Vim-like Interactive Editor `

    Implemented a basic text editor (edit) command similar to Vim using termios for raw input and cursor manipulation via ANSI escape codes. This editor supports in-terminal, multi-line text editing with basic file operations.

Features:
- Load, modify, and save text files
- Cursor navigation with arrow keys
- Line-level editing including:
    - :w (write), :q (quit), :wq (save & quit)
    - :up/:down to move between lines
    - :d to delete a line, :n to insert a new line
- Supports editing up to 100 lines, with 256 characters per line
- Displays a visual prompt showing the current line being edited

Command edit <filename>
This showcases advanced terminal I/O handling, raw mode editing, and a mini text editor within PennOS.


## General Comments

PennOS provided a comprehensive learning experience in systems programming and OS design. Key takeaways include:

	- Hands-on understanding of low-level OS components such as schedulers, system calls, signals, and file systems.
	- Deepened knowledge of process control, non-blocking I/O, and terminal manipulation using termios and fcntl.
	- Development of practical utilities like an interactive shell and a mini text editor reinforced modular design and user-kernel separation.
	- Emphasis on debugging, testing, and synchronization offered valuable insight into real-world OS development challenges.

This project has significantly contributed to our appreciation of modern operating systems and their intricacies. Thank you Professor Travis, Professor Joel and TAs!
