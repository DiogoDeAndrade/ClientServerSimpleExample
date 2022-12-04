
#include "Common.h"

// Structure for a node in a linked list structure in C
typedef struct _Connection
{
    int                 client_id;
    SOCKET              socket;
    uintptr_t           thread_id;
    struct _Connection* next;
} Connection;

// This stores all connections as a C list
Connection* g_connections = NULL;

// Mutex (binary semaphore) for critical region control
HANDLE  connections_mutex;

// This is the entrypoint for the threads handling data from each individual connection
void connection_thread(void* param)
{
    // Parameter is whatever is passed to _beginthread
    Connection* connection_data = (Connection*)param;
    int         client_id = connection_data->client_id;
    SOCKET      socket = connection_data->socket;

    printf("Starting thread for connection %i\n", client_id);

    char        nickname[64];
    sprintf_s((char*)&nickname, 64, "Client %i", client_id);

    int exit = 0;
    while (!exit)
    {
        // Receive type and length of message. We use MSG_PEEK so that we don't remove the data from the socket
        // data, and can read it again when we know the type of message
        MessageBase header;
        int bytes_received = recv(socket, (char*)&header, sizeof(MessageBase), MSG_PEEK);
        if (bytes_received == -1)
        {
            // Socket failed, close and exit
            break;
        }
        else if (bytes_received == sizeof(MessageBase))
        {
            // Handle messages
            switch (header.msg_type)
            {
                case MSG_SETNICK:
                    {
                        MessageSetNick  msg_set_nick;
                        int bytes_received = recv(socket, (char*)&msg_set_nick, sizeof(MessageSetNick), 0);
                        if (bytes_received == sizeof(MessageSetNick))
                        {
                            printf("Client %i changed nickname from %s to %s...\n", client_id, nickname, msg_set_nick.nickname);
                            sprintf_s((char*)&nickname, 64, "%s", msg_set_nick.nickname);
                        }
                    }
                    break;
                case MSG_TEXT_FROM_CLIENT:
                    {
                        MessageTextFromClient  msg_text;
                        int bytes_received = recv(socket, (char*)&msg_text, sizeof(MessageTextFromClient), 0);
                        if (bytes_received == sizeof(MessageTextFromClient))
                        {
                            printf("[%s] %s\n", nickname, msg_text.text);

                            // Send this text to all the other clients (except the client that sent the data)
                            // We're touching a structure shared by multiple threads, so we need to shield this access with the mutex
                            WaitForSingleObject(connections_mutex, INFINITE);

                            Connection* current = g_connections;
                            while (current != NULL)
                            {
                                if (current != connection_data)
                                { 
                                    MessageTextFromServer msg_to_send;
                                    msg_to_send.msg_type = MSG_TEXT_FROM_SERVER;
                                    msg_to_send.msg_length = sizeof(MessageTextFromServer);
                                    sprintf_s((char*)&msg_to_send.text, 1024, "%s", msg_text.text);
                                    sprintf_s((char*)&msg_to_send.nickname, 64, "%s", nickname);

                                    send(current->socket, (char*)&msg_to_send, sizeof(MessageTextFromServer), 0);
                                }
                                
                                current = current->next;
                            }

                            // We no longer need to have the mutex locked, so release it so others can lock it
                            ReleaseMutex(connections_mutex);
                        }
                    }
                    break;
                case MSG_EXIT:
                    {
                        MessageExit  msg_exit;
                        int bytes_received = recv(socket, (char*)&msg_exit, sizeof(MessageExit), 0);
                        if (bytes_received == sizeof(MessageExit))
                        {
                            printf("Client %i requested exit...\n", client_id);
                            exit = 1;
                        }
                    }
                    break;
            }
        }
    }

    printf("Ending thread for connection %i\n", client_id);

    // In *nix/OSX, the following function is called close, not closesocket
    closesocket(socket);

    // We will now remove this structure from the list of structures, but
    // we're touching a structure shared by multiple threads, so we need to shield this access with the mutex
    WaitForSingleObject(connections_mutex, INFINITE);

    if (connection_data == g_connections)
    {
        g_connections = connection_data->next;
    }
    else
    {
        Connection* current = g_connections;
        Connection* prev = current;
        while (current != NULL)
        {
            if (current == connection_data)
            {
                prev->next = current->next;
                break;
            }

            prev = current;
            current = current->next;
        }
    }
    free(connection_data);

    // We no longer need to have the mutex locked, so release it so others can lock it
    ReleaseMutex(connections_mutex);
}

int main(int argc, char** argv)
{
    // This will be the main thread, it just listens to requests on a socket

    // Create the mutex (binary semaphore) object - this will stop multiple threads of
    // accessing the same information and possibly accessing corrupted data
    // Data that is shared between threads needs to be protected so that corrupted or
    // invalid data is not accessed
    // Unfortunately, Win32 mutexes are completely different from OSX/*nix systems
    // C++ 14 solves this to an extend with the STL
    connections_mutex = CreateMutex(NULL, FALSE, NULL);

    // In Windows, we need to initialize WinSock, in OSX/*nix systems this is not required
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) 
    {
        printf("WSAStartup failed!\n");
        return -1;
    }

    // Create a listen socket
    printf("Creating socket...\n");
    SOCKET main_socket = socket(AF_INET, SOCK_STREAM, 0);

    // Setup the structure for the listening address - can  listen on any ethernet device, in the given port
    struct sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(LISTEN_PORT);

    // Binding the socket to the address
    printf("Binding to %i...\n", LISTEN_PORT);
    bind(main_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    printf("Socket is now bound to port %i\n", LISTEN_PORT);

    // Setup the listening
    printf("Listening...\n");
    if (listen(main_socket, 10) == -1) 
    {
        printf("Failed to listen\n");
        // In Windows, need to cleanup WinSock
        WSACleanup();
        return -1;
    }

    int client_id = 0;

    while (1)
    {
        // Wait for a connection to be established
        SOCKET new_socket = accept(main_socket, (struct sockaddr*)NULL, NULL); // accept awaiting request
        printf("Received a new connection...\n");

        // Create structure to store thread/socket information - this is implemented as a simple C list
        // We're touching a structure shared by multiple threads, so we need to shield this access with
        // the mutex
        WaitForSingleObject(connections_mutex, INFINITE);

        Connection* new_connection = (Connection*)malloc(sizeof(Connection));
        new_connection->client_id = client_id++;
        new_connection->socket = new_socket;
        new_connection->thread_id = -1;
        new_connection->next = g_connections;

        g_connections = new_connection;
                
        // Create a thread to handle the input from this socket - last parameter is passed to the thread function
        new_connection->thread_id = _beginthread(connection_thread, 0, new_connection);

        // After the critical area is done, we need to release the mutex (or else nobody else will be able to lock)
        ReleaseMutex(connections_mutex);
    }

    // In Windows, need to cleanup WinSock
    WSACleanup();

    return 0;
}
