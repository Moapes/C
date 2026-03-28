#include "./command-redir.h"


//this function will act in the following way
//if it has some sort of attribute of input to it, we will take it as a string that we will inset into the mFile by reading the damn hIn handle
//if it has some sort of attribute of output to it, we will ofc copy the whole content of the file and send it over the pipe / redirection or whatever
bool redir(ShellCommand* cmd)
{
    DWORD execAttrs = cmd->execAttrs;
    
}