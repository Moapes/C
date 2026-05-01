#include <iostream>      // For cout/cin
#include <string>        // For better string handling
#include <vector>        // For managing multiple client threads later
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#pragma comment(lib, "ws2_32.lib")
#include <thread> // For creating workers
#include <mutex>  // For the "Bathroom Key"

#include <sqlite3.h>
#include <iostream>


#define SESSION_TOKEN_SIZE 32


enum CommandID {
    CMD_LOGIN = 1,
    CMD_SIGNIN = 2,
    CMD_LOGOUT = 3,
};


typedef struct {
    std::string user_name;
    std::string user_pswd;
    std::string session_token;
    size_t user_name_len;
    size_t user_pswd_len;
    sockaddr_in client_addr;
    SOCKET client_socket;
}sqlUser_data;

typedef struct {
    SOCKET client_socket;
    sockaddr_in client_addr;
}setup_user_data;

std::vector<sqlUser_data*> active_users;
std::mutex active_users_mutex;



#define MAX_COMMAND_LENGTH 6

void generate_session_token(char* token_buff)
{

}

void handle_client_enter(setup_user_data* data)//setup phase when the client contacts the server for the first time
{
    int bytesReceived = 0;
    int total_bytes_recieved = 0;
    int pswd_len = 0;
    int username_len = 0;
    int total_len = 0;
    SOCKET c_sock = data->client_socket;

    int commandType = 0;
    int result = recv(c_sock,(char*)&commandType,sizeof(int),0);
    if(result == SOCKET_ERROR)
    {
        std::cout << "Connection error. " << std::endl;
        delete data;
        return;
    }
    if(result == 0)
    {
        std::cout << "client disconnected. " << std::endl;
        delete data;
        return;
    }


    result = recv(c_sock,(char*)&username_len,sizeof(int),0);
    if(result == SOCKET_ERROR)
    {
        std::cout << "Connection error. " << std::endl;
        delete data;
        return;
    }
    if(result == 0)
    {
        std::cout << "client disconnected. " << std::endl;
        delete data;
        return;
    }
    result = recv(c_sock,(char*)&pswd_len,sizeof(int),0);
    if(result == SOCKET_ERROR)
    {
        std::cout << "Connection error. " << std::endl;
        delete data;
        return;
    }
    if(result == 0)
    {
        std::cout << "client disconnected. " << std::endl;
        delete data;
        return;
    }
    int total_len = username_len + pswd_len;
    char* buffer = new char[total_len];
    char* user_creds_buff = new char[total_len];

    std::string username;
    std::string pswd;

    while (total_bytes_recieved < total_len) {
        bytesReceived = recv(data->client_socket, buffer, sizeof(buffer) - 1, 0);
        int c = 0;
        for(int i = total_bytes_recieved; i < total_len; i++)
        {
            if(buffer[c] == '\0' || buffer[c] == '\n' || buffer[c] == '\t')
            {
                break;
            }
            user_creds_buff[i] = buffer[c];
            c++;
        }

        if (bytesReceived == SOCKET_ERROR) {
            std::cout << "Connection error " << std::endl;
            break;
        }
        if (bytesReceived == 0) {
            std::cout << "Client disconnected. " << std::endl;
            break;
        }
        total_bytes_recieved += c;       
    }
    if(total_bytes_recieved < total_len)
    {
        std::cout << "Error recieving creds" << std::endl;
    }
    else
    {
        std::string username(buffer,username_len);
        std::string pswd(buffer + username_len,pswd_len);
        if(commandType == CMD_LOGIN)
        {

        }
    }


    delete[] buffer;
    delete data;
}


void add_active_user(void* data)
{

}

void handle_client_session(void* data)
{

}


int main() {
    // 1. Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "Winsock init failed." << std::endl;
        return 1;
    }

    // 2. Create the Socket
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        std::cout << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // 3. Setup Address Structure
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(8080); // Host TO Network Short

    // 4. Bind
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cout << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    // 5. Listen
    if (listen(server_fd, 3) == SOCKET_ERROR) {
        std::cout << "Listen failed." << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

        
    sqlite3* db;
    int rc = sqlite3_open("users.db",&db);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open DB!" << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1; // Kill the server if the DB is missing
    }
    std::cout << "C++ Server is live on Windows port 8080..." << std::endl;

    //Listening process -> once a mf is found we send him to the login/signin phase:
    while(1)
    {
        sockaddr_in clientAddr;
        int clientLen = sizeof(clientAddr);

        SOCKET clientSocket = accept(server_fd, (struct sockaddr*)&clientAddr,&clientLen);

        if(clientSocket == INVALID_SOCKET)
        {
            std::cout << "Accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }
        char readable_client_ip[INET_ADDRSTRLEN]; 
        inet_ntop(AF_INET,&clientAddr.sin_addr,readable_client_ip,INET_ADDRSTRLEN);
        std::cout << "New Client Connected[ " << readable_client_ip << " ]" << std::endl;
        setup_user_data* new_user = new setup_user_data;
        new_user->client_addr = clientAddr;
        new_user->client_socket = clientSocket;
        std::thread t(handle_client_enter, new_user);

        t.detach();
    }



    // Cleanup (usually at the end of the program)
    closesocket(server_fd);
    WSACleanup();
    return 0;
}



