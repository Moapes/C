#include "./command-cd.h"
//in the cd, we will reallocate the amount of bytes needed to hold the new cwd and switch the main's pointer to it(and get rid of the old one)
bool cd(ShellCommand* currCommand,char** cwd)
{
    int newLen = 0;
    char* newCWDPtr = {0};
    HANDLE source = currCommand->hIn;
    char tempCWDBuff[1024] = {0};
    DWORD execAttrs = currCommand->execAttrs;
    if(execAttrs & ATTR_PIPE_IN)
    {
        char readBuff[1024];
        DWORD bytesRead;
        while(ReadFile(source,readBuff,sizeof(readBuff),&bytesRead,NULL))
        {
            readBuff[bytesRead] = '\0';

            strcat(tempCWDBuff,readBuff);
        }
        newLen = strlen(tempCWDBuff);
        newCWDPtr = (char*)malloc(newLen + 1);
        strcpy(newCWDPtr,tempCWDBuff);
        free(*cwd);
        *cwd = newCWDPtr;
    }
    else
    {
        newLen = strlen(currCommand->path1);//check how much we need to allocate for the new cwd
        newCWDPtr = (char*)malloc(newLen + 1);//allocated it( +1 for null terminator)
        strcpy(newCWDPtr,currCommand->path1);//copy the contents to the new pointer
        free(*cwd);//free the old pointer
        *cwd = newCWDPtr;//new pointerr
    }
    if(execAttrs & ATTR_PIPE_OUT)
    {   
        printf("change directory command output cannot be redirected / piped\n");
        CloseHandle(currCommand->hOut);
        return false;
    }
}   


