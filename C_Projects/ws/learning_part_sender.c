// #ifndef WIN32_LEAN_AND_MEAN
// #define WIN32_LEAN_AND_MEAN
// #endif
// // h = Host (Your PC)

// // n = Network (The Wire)

// // s = Short (16-bit, for Ports)

// // l = Long (32-bit, for IP Addresses)
// #include <stdio.h>
// #include <winsock2.h> //general socket functions
// #include <windows.h>
// #include <ws2tcpip.h> //strycts for IP/Internet
// #pragma comment(lib,"ws2_32.lib") //help funcs like inet_addr


// int main()
// {   
//     WSADATA wsaData;
//     int iResult;


//     iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
//     if(iResult != 0)
//     {
//         printf("WSAStartup Failed! %d\n",iResult);
//         return 1;
//     }


//     SOCKET ListenSocket = INVALID_SOCKET;
//     ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

//     if(ListenSocket == INVALID_SOCKET)
//     {
//         printf("Error creating socket: %ld\n",WSAGetLastError());
//         WSACleanup();
//         return 1;
//     }


//     struct sockaddr_in sender;
//     char s_ip = "127.0.0.1";
//     char* sender_ip = s_ip;
//     u_short service_port = 8000;
//     sender.sin_family = AF_INET;

//     sender.

//     return 0;
// }