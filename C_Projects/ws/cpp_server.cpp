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
    CMD_SIGNUP = 2,
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
    sqlite3* db;
}sqlUser_data;

typedef struct {
    SOCKET client_socket;
    sockaddr_in client_addr;
    sqlite3* db;
}setup_user_data;

std::vector<sqlUser_data*> active_users;
std::mutex active_users_mutex;

#define LOGIN_SUCCESS 1
#define USERNAME_EXISTS 2
#define USERNAME_INCORRECT 3
#define PASSWORD_INCORRECT 4
#define USER_LOGGED 5

#define MAX_COMMAND_LENGTH 6

void generate_session_token(char* token_buff)
{

}


int validate_login(sqlite3* db, std::string targetUsername, std::string targetPassword)
{
    sqlite3_stmt* stmt;
    std::string sql = "SELECT USERNAME, PASSWORD FROM USERS WHERE NAME = ?;";

    int rc = sqlite3_prepare_v2(db, sql.c_str(),-1,&stmt,nullptr);
    sqlite3_bind_text(stmt,1,targetUsername.c_str(),-1,SQLITE_STATIC);

    bool creds_correct = false;

    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char* name = sqlite3_column_text(stmt,1);
        const unsigned char* pass = sqlite3_column_text(stmt,2);

        std::string curr_user((const char*)name);
        std::string curr_pswd((const char*)pass);
        if(curr_user == targetUsername)
        {
            if(curr_pswd != targetPassword)
            {
                std::cout << "Wrong Password" << std::endl;
                sqlite3_finalize(stmt);
                return PASSWORD_INCORRECT;
            }
            creds_correct = true;
        }
    }   
    if(!creds_correct)
    {
        std::cout << "Wrong Username" << std::endl;
        sqlite3_finalize(stmt);
        return USERNAME_INCORRECT;
    }

    for(auto& user : active_users)
    {
        if(user->user_name == targetUsername)
        {
            std::cout << "User Already logged in" << std::endl;
            sqlite3_finalize(stmt);
            return USER_LOGGED;
        }
    }

    sqlite3_finalize(stmt);
    return LOGIN_SUCCESS;
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
        sqlite3* db = data->db;
        if(commandType == CMD_LOGIN)
        {
            //check if the username exists and if not--> send error msg..
            char* response;
            int isValid = validate_login(db,username,pswd);
            if(isValid) 
            {
                response = "bud.. you lwky made it!, ur logged in!";
                send(c_sock,response,strlen(response),0);
                sqlUser_data* user_data = create_user(data,username,pswd,username_len,pswd_len);
                handle_client_session(user_data);
            }
            else
            {
                switch (isValid){
                    case USERNAME_INCORRECT:
                        response = "Incorrect Username bud.. ";
                        break;
                    case PASSWORD_INCORRECT:
                        response = "Incorrect Password bud.. ";
                        break;
                    case USER_LOGGED:
                        response = "Someone Already logged in this account bud.. ";
                        break;
                }
                send(c_sock,response,strlen(response),0);
            }
        }
        else if(commandType == CMD_SIGNUP)
        {
            
        }
        else
        {
            send(c_sock,"Wrong command.. ",strlen("Wrong command"),0);
        }

    }


    delete[] buffer;
    delete data;
}


sqlUser_data* create_user(setup_user_data* data,std::string username, std::string password, int username_len ,int password_len)
{
    char sess_token[SESSION_TOKEN_SIZE];
    generate_session_token(sess_token);
    sqlUser_data* new_user = new sqlUser_data;
    new_user->db = data->db;
    new_user->client_addr = data->client_addr;
    new_user->client_socket = data->client_socket;
    std::string s_token(sess_token);
    new_user->session_token = s_token;
    new_user->user_name = username;
    new_user->user_name_len = username_len;
    new_user->user_pswd = password;
    new_user->user_pswd_len = password_len;

    std::lock_guard<std::mutex> guard(active_users_mutex);
    active_users.push_back(new_user);

    return new_user;
}

void handle_client_session(sqlUser_data* user_data)
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
        new_user->db = db;
        std::thread t(handle_client_enter, new_user);

        t.detach();
    }



    // Cleanup (usually at the end of the program)
    sqlite3_close(db);
    closesocket(server_fd);
    WSACleanup();
    return 0;
}



