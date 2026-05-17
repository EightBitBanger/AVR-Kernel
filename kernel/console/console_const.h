#ifndef CONSOLE_CONST_H
#define CONSOLE_CONST_H

#ifdef KERNEL_PLATFORM_AVR
  static const char* msg_bad_command     = "Bad cmd or filename\n";
#endif

#ifdef KERNEL_PLATFORM_X86
  static const char* msg_bad_command     = "Bad command or filename\n";
#endif

static const char* msg_dir_not_found   = "Directory not found\n";
static const char* msg_dir_error       = "Directory error\n";
static const char* msg_dir_sym         = "<DIR>";
static const char* msg_dir_root        = "/>";

#endif
