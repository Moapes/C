#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// h = Host (Your PC)

// n = Network (The Wire)

// s = Short (16-bit, for Ports)

// l = Long (32-bit, for IP Addresses)
#include <stdio.h>
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

    clientSocket = accept(ListenSocket,(struct sockaddr*)&accepted_socket,(int*)&accepted_size);

    if(clientSocket == INVALID_SOCKET)
    {
        printf("Accept failed: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    char recv_buff[1024] = {0};
    memset(recv_buff,0,sizeof(recv_buff));
    int recv_result = recv(clientSocket, recv_buff,sizeof(recv_buff),0);
    
    while(recv_result > 0)
    {
        if(strlen(recv_buff) != 0)
        {
            char ip_string[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET,&accepted_socket.sin_addr.s_addr,ip_string,INET_ADDRSTRLEN);
            printf("Data from [%s]: %s \n", ip_string, recv_buff);
        }
        memset(recv_buff,0,sizeof(recv_buff));
        recv_result = recv(clientSocket, recv_buff, sizeof(recv_buff),0);
    }
    if(recv_result == 0)
    {
        printf("connection gracefully ended\n");
    }
    if(recv_result < 0)
    {
        printf("client not found\n");
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}