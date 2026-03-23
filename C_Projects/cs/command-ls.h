#ifndef LS_COMMAND
#include "./custom_shell.h"


void printLinkTarget(const char* linkPath);
void ls_wrapped(ShellCommand* currCommand,char* cwd,char* search_path,int offset);
void lsNoArgs(ShellCommand* currCommand,char* cwd);
bool ls(ShellCommand* currCommand,char* cwd);

#endif //LS_COMMAND