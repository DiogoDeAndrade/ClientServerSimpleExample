
#include "Common.h"

int main(int argc, char** argv)
{
    // The client is single threaded

#ifdef _WIN32
    // In Windows, we need to initialize WinSock, in OSX/*nix systems this is not required
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        printf("WSAStartup failed!\n");
        return -1;
    }
#endif

    // Creating connection socket
    printf("Creating socket...\n");
    SOCKET main_socket = 0;
    if ((main_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        printf("\n Socket creation error \n");

#if _WIN32
        // In Windows, need to cleanup WinSock
        WSACleanup();
#endif

        return -1;
    }

    // Setup the structure for the listening address - can  listen on any ethernet device, in the given port
    // The connection IP I'm using is localhost, you probably want to add some command
    // line parsing to get the correct address for the server
    char target_name[256];
    sprintf_s((char*)&target_name, 256, "127.0.0.1");

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(LISTEN_PORT);
    inet_pton(AF_INET, target_name, &serv_addr.sin_addr);

    printf("Connecting to %s...\n", target_name);
    int result = connect(main_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (result < 0)
    {
        printf("Connection failed!");

#if _WIN32
        // In Windows, need to cleanup WinSock
        WSACleanup();
#endif

        return -1;
    }

    // Setup socket to be non-blocking - in Windows this is required because we can't 
    // specify a recv to be non-blocking like we do in OSX/*nix systems
    int nonblock = 1;
    ioctlsocket(main_socket, FIONBIO, &nonblock);

    // 
    printf("Connected to %s...\n", target_name);
    printf("Type in text to send, or use the following commands:\n");
    printf("/nick <nickname>: sets the nickname to use\n");
    printf("/exit: close the connection\n");

    int exit = 0;
    while (!exit)
    {
        char buffer[1024];

        fgets((char*)&buffer, 1024, stdin);

        // Trick to remove \n from what is read from the stdin (fgets preserves the trailing \r or \n)
        buffer[strcspn(buffer, "\r\n")] = 0;

        if (!_strnicmp(buffer, "/nick ", 6))
        {
            // Set nickname
            MessageSetNick msg_set_nick;
            msg_set_nick.msg_type = MSG_SETNICK;
            msg_set_nick.msg_length = sizeof(MessageSetNick);
            sprintf_s((char*)&msg_set_nick.nickname, 64, "%s", (char*)buffer + 6);

            int bytes_sent = send(main_socket, (char*)&msg_set_nick, sizeof(MessageSetNick), 0);
            if (bytes_sent != sizeof(MessageSetNick))
            {
                // Something went wrong sending the message, exit application
                exit = 1;
            }
        }
        else if (!_strnicmp(buffer, "/exit", 5))
        {
            // Exit application
            break;
        }
        else if (strlen(buffer) != 0)
        {
            // Send message, if there's a message to send
            MessageTextFromClient msg_text;
            msg_text.msg_type = MSG_TEXT_FROM_CLIENT;
            msg_text.msg_length = sizeof(MessageTextFromClient);
            sprintf_s((char*)&msg_text.text, 1024, "%s", (char*)buffer);

            int bytes_sent = send(main_socket, (char*)&msg_text, sizeof(MessageTextFromClient), 0);
            if (bytes_sent != sizeof(MessageTextFromClient))
            {
                // Something went wrong sending the message, exit application
                exit = 1;
            }
        }

        // Check if there are any messages to receive from the server - process until there are no more messages
        int retry = 1;
        while (retry)
        {
            // Reset retry flag
            retry = 0;

            MessageBase header;
            int bytes_received = recv(main_socket, (char*)&header, sizeof(MessageBase), MSG_PEEK);
            // Check if this recv would block if the socket was blocking
            if (bytes_received > 0)
            {
                if (bytes_received == sizeof(MessageBase))
                {
                    // Only one message type supported on client
                    if (header.msg_type == MSG_TEXT_FROM_SERVER)
                    {
                        MessageTextFromServer msg_text;
                        int bytes_received = recv(main_socket, (char*)&msg_text, sizeof(MessageTextFromServer), 0);
                        if (bytes_received == sizeof(MessageTextFromServer))
                        {
                            printf("[%s] %s\n", msg_text.nickname, msg_text.text);
                            // Valid message received, retry
                            retry = 1;
                            exit = 0;
                        }
                    }
                    else
                    {
                        // Received something that isn't supported, disconnect application
                        exit = 1;
                    }
                }
            }
            else if (bytes_received < 0)
            {
                // Check if there is an error
                if (errno == 0)
                {
                    // No error, continue
                }
                else
                {
                    // Error on socket, disconnect application
                    exit = 1;
                }
            }
        }
    }

    // Send exit message to server
    MessageExit msg_exit;
    msg_exit.msg_type = MSG_EXIT;
    msg_exit.msg_length = sizeof(MessageExit);

    send(main_socket, (char*)&msg_exit, sizeof(MessageExit), 0);
    
    // In *nix/OSX, the following function is called close, not closesocket
#ifdef _WIN32
    closesocket(main_socket);
#else
    close(new_socket);
#endif

#if _WIN32
    // In Windows, need to cleanup WinSock
    WSACleanup();
#endif

    return 0;
}
