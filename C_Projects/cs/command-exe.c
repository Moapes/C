#include "./command-exe.h"


bool exe(ShellCommand* currCommand)
{
    STARTUPINFO si;

    si.hStdInput  = (currCommand->hIn  != NULL) ? currCommand->hIn  : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = (currCommand->hOut != NULL) ? currCommand->hOut : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if(!CreateProcessA(
        NULL,
        currCommand->path1,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    ))
    {
        printf("Process Creation Failed (%d).\n",GetLastError());
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    printf("\nProcess finished. Returning to shell...\n");
    return true;
}


