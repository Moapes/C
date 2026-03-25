#include "./command-cd.h"
//in the cd, we will reallocate the amount of bytes needed to hold the new cwd and switch the main's pointer to it(and get rid of the old one)
void cd(ShellCommand* currCommand,char** cwd)
{
    int newLen = strlen(currCommand->path1);//check how much we need to allocate for the new cwd
    char* newCWDPtr = (char*)malloc(newLen + 1);//allocated it( +1 for null terminator)
    strcpy(newCWDPtr,currCommand->path1);//copy the contents to the new pointer
    free(*cwd);//free the old pointer
    *cwd = newCWDPtr;//new pointerr
}

