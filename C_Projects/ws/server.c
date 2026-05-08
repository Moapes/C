#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>

#include "sqlite3.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// h = Host (Your PC)

// n = Network (The Wire)

// s = Short (16-bit, for Ports)
uint64_t ledger = 0;
pthread_mutex_t ledger_lock = PTHREAD_MUTEX_INITIALIZER;

// l = Long (32-bit, for IP Addresses)
#include <stdio.h>
#include <string.h>
#include <winsock2.h> //general socket functions
#include <windows.h>
#include <ws2tcpip.h> //strycts for IP/Internet
#pragma comment(lib,"ws2_32.lib") //help funcs like inet_addr

// struct sockaddr_in
// {
//     short sin_family;
//     unsigned short sin_port;
//     struct in_addr sin_addr;
//     char sin_zero[8];
// };

typedef struct client_data
{
    struct sockaddr_in accepted_socket;
    SOCKET client_socket;
}client_data;

typedef struct user_data
{
    client_data c_data;
    char* username;
    char* password;
    char* bio;
    char* session_token;
}user_data;

#define SIGNUP_SUCCESS 1
#define SIGNUP_INVALID_USERNAME 2
#define LOGIN_SUCCESS 4
#define LOGIN_INVALID_USERNAME 8
#define LOGIN_INVALID_PASSWORD 16

#define MAX_PASSWORD_LENGTH

void generate_session_token(char *token,int length)
{
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    for(int i = 0; i < length; i++)
    {
        int index = rand() % (sizeof(charset) - 1);
        token[i] = charset[index];
    }
    token[length] = '\0';
}

int user_exists(sqlite3 *db, char *username) 
{
    int exists = 0;
    sqlite3_stmt *stmt;
    char query_part = "SELECT username FROM users WHERE username = ";
    char *p_to_query_part = query_part;
    int len_of_query_part = strlen(p_to_query_part);
    int additional_len = strlen("'';");
    char sql[len_of_query_part + strlen(username) + additional_len];
    sprintf(sql,"SELECT username FROM users WHERE username = '%s';",username);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("Failed to prepare statement\n");
        return 0;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = 1; 
    }

    // 4. Cleanup
    sqlite3_finalize(stmt);
    return exists;
} 

int user_signup(char* signup_buff,sqlite3 *db)
{  
    int user_exist = user_exists(db,signup_buff);
    if(user_exist) return SIGNUP_INVALID_USERNAME;

    char new_password[MAX_PASSWORD_LENGTH] = {0};
    char *p_to_password_buff = new_password;
    printf("Assign a password to your new account: ");
    if(fgets(p_to_password_buff,sizeof(new_password),stdin) != NULL)
    {
        p_to_password_buff[strcspn(p_to_password_buff,"\n")] = "\0";
        char* username = signup_buff;
        
        
    }
    
}


int user_login(sqlite3 *db,char* login_buff)
{

}

void* intro_client_stage(void* cl_data,sqlite3 *db)
{

    if(!cl_data) return;
    client_data* c_data = (client_data*)cl_data;
    pthread_mutex_lock(&ledger_lock);
    ledger += 1;
    pthread_mutex_unlock(&ledger_lock);    
    char ip_string[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET,&(c_data->accepted_socket.sin_addr.s_addr),ip_string,INET_ADDRSTRLEN);
    char recv_buff[1024] = {0};
    memset(recv_buff,0,sizeof(recv_buff));
    int recv_result = recv(c_data->client_socket, recv_buff,sizeof(recv_buff),0);
    char* signup = "signup";
    char* login = "login";
    while(recv_result > 0)
    {
        if(strlen(recv_buff) != 0)
        {
            if(strncmp(recv_buff,signup,strlen(signup)))
            {
                int res = user_signup(db,recv_buff + strlen(signup));
                if(res & SIGNUP_SUCCESS)
                {
                    
                }
                else if(res & SIGNUP_INVALID_USERNAME)
                {
                    printf("Username: %s already exists\n",recv_buff + strlen(signup));
                }
            }
            else if(strncmp(recv_buff,login,strlen(login)))
            {
                int res = user_login(db,recv_buff + strlen(login));
            }
            else
            {

                char *p = strpbrk(recv_buff," \n");
                *p = '\n';
                printf("Invalid Command : %s | From: %s\n",recv_buff,ip_string);
            }
            

        }
        memset(recv_buff,0,sizeof(recv_buff));
        recv_result = recv(c_data->client_socket, recv_buff, sizeof(recv_buff),0);
    } 
    if(recv_result == 0) printf("Connection ended gracefully with %s\n",ip_string);
    else if(recv_result < 0) printf("client not found\n");

    closesocket(c_data->client_socket);
    free(c_data);
    return NULL;   
}  



void* handle_client(void* c_data)
{  
    client_data* c_Data = c_data; 
    if(!c_Data) return;

    char recv_buff[1024] = {0};
    memset(recv_buff,0,sizeof(recv_buff));
    int recv_result = recv(c_Data->client_socket, recv_buff,sizeof(recv_buff),0);
    
    while(recv_result > 0)
    {
        if(strlen(recv_buff) != 0)
        {
            char ip_string[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET,&(c_Data->accepted_socket.sin_addr.s_addr),ip_string,INET_ADDRSTRLEN);
            printf("Data from [%s]: %s \n", ip_string, recv_buff);
        }
        memset(recv_buff,0,sizeof(recv_buff));
        recv_result = recv(c_Data->client_socket, recv_buff, sizeof(recv_buff),0);
    }
    if(recv_result == 0)
    {
        printf("connection gracefully ended\n");
    }
    if(recv_result < 0)
    {
        printf("client not found\n");
    }
    closesocket(c_Data->client_socket);
    free(c_Data);
    return NULL;
}

int main()
{
    WSADATA wsaData;
    int iResult;


    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if(iResult != 0)
    {
        printf("WSAStartup Failed! %d\n",iResult);
        return 1;
    }


    SOCKET ListenSocket = INVALID_SOCKET;
    ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(ListenSocket == INVALID_SOCKET)
    {
        printf("Error creating socket: %ld\n",WSAGetLastError());
        WSACleanup();
        return 1;
    }

    struct sockaddr_in service;
    char s_ip[INET_ADDRSTRLEN] = "127.0.0.1";
    char* service_ip = s_ip; 
    u_short service_port = 8000;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr(service_ip);
    service.sin_port = htons(service_port);

    iResult = bind(ListenSocket,(SOCKADDR*) &service, sizeof(service));
    if(iResult != 0)
    {
        printf("Socket binding failed %d\n",iResult);
        WSACleanup();
        return 1;
    }

    iResult = listen(ListenSocket, SOMAXCONN);
    if(iResult != 0)
    {
        printf("Socket Listening startup failed %d\n",iResult);
        WSACleanup();
        return 1;
    }
    struct sockaddr_in accepted_socket;
    int accepted_size = sizeof(accepted_socket);
    SOCKET clientSocket = INVALID_SOCKET;

    sqlite3 *db;

    int rc = sqlite3_open("users.db",&db);
    if(rc != SQLITE_OK)
    {
        fprintf(stderr,"Cannot open database: %s\n", sqlite3_errmsg(db));
        WSACleanup();
        return 1;
    }  


    while(1)
    {
        clientSocket = accept(ListenSocket,(struct sockaddr*)&accepted_socket,(int*)&accepted_size); 
        if(clientSocket == INVALID_SOCKET)
        {
            printf("Accept failed: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            sqlite3_close(db);
            return 1;
        }
        pthread_t thread_id;
        client_data* c_data = malloc(sizeof(struct client_data));
        memset(c_data,0,sizeof(struct client_data));
        c_data->client_socket = clientSocket;
        c_data->accepted_socket = accepted_socket;
        if(pthread_create(&thread_id, NULL, intro_client_stage(client_data,), (void*)c_data))
        {
            printf("Thread creation failed\n");
        }
        pthread_detach(thread_id);
        printf("%d\n",ledger);
        memset(&accepted_socket,0,sizeof(accepted_socket));
    }

    closesocket(clientSocket);
    WSACleanup();
    sqlite3_close(db);
    return 0;
}