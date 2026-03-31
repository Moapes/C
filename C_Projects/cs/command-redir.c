#include "./command-redir.h"


//this function will act in the following way
//if it has some sort of attribute of input to it, we will take it as a string that we will inset into the mFile by reading the damn hIn handle
//if it has some sort of attribute of output to it, we will ofc copy the whole content of the file and send it over the pipe / redirection or whatever
//
bool redir(ShellCommand* cmd)
{
    DWORD execAttrs = cmd->execAttrs;
    char contentBuffer[1024];
    bool sourceFromFile = false;
    if(execAttrs | ATTR_REDIR_IN)
    {
        char inputBuffer[1024];
        DWORD bytesReaden;
        HANDLE source = cmd->hIn;
        //first we read the file onto a buffer
        while(ReadFile(source,inputBuffer,sizeof(inputBuffer),&bytesReaden,NULL))
        {
            inputBuffer[bytesReaden] = '\0';
            strcat(contentBuffer,inputBuffer);
        }    
    }
    else//that means that this file is the source
    {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };     
        HANDLE source = CreateFile(
            cmd->path1,                // Path to the file
            GENERIC_WRITE,               // Open for writing
            FILE_SHARE_READ,             // Allow others to read while we write
            &sa,                         // Security attributes (Inheritable)
            CREATE_ALWAYS,               // Equivalent to ">" (Overwrite/Create)
            FILE_ATTRIBUTE_NORMAL,       // Normal file
            NULL                         // No template
        );

        if(!source) return false;
        //actually write to the file
        DWORD bytesReaden;
        char inputBuffer[1024] = {0};
        while(ReadFile(source,inputBuffer,sizeof(inputBuffer),&bytesReaden,NULL))
        {
            inputBuffer[bytesReaden] = '\0';
            strcat(contentBuffer,inputBuffer);
        }
        CloseHandle(source);         
    }
    if(execAttrs | ATTR_REDIR_OUT)//make the contentBuffer write to the out handle
    {
        HANDLE destination = cmd->hOut;
        DWORD bytesWritten;
        if(!WriteFile(destination,contentBuffer,sizeof(contentBuffer),&bytesWritten,NULL))
        {
            return false;
        }
        return true;
    }
    //write the contentBuffer directly to the path we got already
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE }; 

    HANDLE destination = CreateFile(
        cmd->path1,                // Path to the file
        GENERIC_WRITE,               // Open for writing
        FILE_SHARE_READ,             // Allow others to read while we write
        &sa,                         // Security attributes (Inheritable)
        CREATE_ALWAYS,               // Equivalent to ">" (Overwrite/Create)
        FILE_ATTRIBUTE_NORMAL,       // Normal file
        NULL                         // No template
    );

    if(!destination) return false;
    //actually write to the file
    DWORD bytesWritten;
    if(!WriteFile(destination,contentBuffer,sizeof(contentBuffer),&bytesWritten,NULL))
    {
        return false;
    } 
    return true;    
}

