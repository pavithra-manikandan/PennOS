#include "./userfunctions/user_functions.h"

#ifndef CT_H_
#define CT_H_

// Command table structure
typedef struct {
  const char* name;
  const char* description;
  void* (*function)(void*);
  bool is_builtin;  // run directly in shell
} command_t;

extern command_t command_table[];
extern int number_commands;

#endif  // CT_H_