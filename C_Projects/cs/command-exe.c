#include "./command-exe.h"


void exe(ShellCommand* currCommand)
{
    STARTUPINFO si;
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
        return;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    printf("\nProcess finished. Returning to shell...\n");
}


