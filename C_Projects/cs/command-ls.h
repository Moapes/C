#ifndef LS_COMMAND
#include "./custom_shell.h"


void printLinkTarget(const char* linkPath);
bool ls_wrapped(ShellCommand* currCommand,char* cwd,char* search_path,int offset);
bool lsNoArgs(ShellCommand* currCommand,char* cwd,char* search_path);
bool ls(ShellCommand* currCommand,char* cwd);

#endif //LS_COMMAND