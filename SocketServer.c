#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <time.h>

#define MAX_CLIENTS 5

// Structure to store client information
struct Client {
    int socket;
    char name[256];  // Assuming a maximum name length of 255 characters
    char ip[INET_ADDRSTRLEN]; // To store the client's IP address
    int Id;
};
struct Client clients[MAX_CLIENTS];

// Function prototypes
void *handle_client(void *arg);
int calculateParity(const char *message);
int extractParity(const char *message);
void extractMessageWithoutParity(const char *input, char *output);
void cmpParity(const char*message,int parity);
int rndCorrupt( char clientname[] ,char* buffer );

int main() {
    // Seed the random number generator
    srand(time(NULL));

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    // Variables for server and client sockets
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_len = sizeof(client_addr);
    pthread_t client_threads[MAX_CLIENTS];
    int num_clients = 0;

    // Create a socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        fprintf(stderr, "Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Set up the server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(12345);  // Choose a port

    // Bind the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr, "Bind failed: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
        fprintf(stderr, "Listen failed: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Main loop for accepting clients
    while (1) {
        // Accept a connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1) {
            fprintf(stderr, "Accept failed: %d\n", WSAGetLastError());
            continue;
        }

        // Get the client's name (assumes the client sends its name as the first message)
        char name[256];
        ssize_t name_length = recv(client_socket, name, sizeof(name), 0);
        if (name_length > 0) {
            name[name_length] = '\0';
        } else {
            fprintf(stderr, "Failed to receive client name\n");
            closesocket(client_socket);
            continue;
        }

        // Store the client information
        if (num_clients < MAX_CLIENTS) {
            clients[num_clients].socket = client_socket;
            strncpy(clients[num_clients].name, name, sizeof(clients[num_clients].name));

            // Get the client's IP address and store it
            if (client_addr.sin_family == AF_INET) {
                struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
                const char *client_ip = inet_ntop(AF_INET, &(ipv4->sin_addr), clients[num_clients].ip, INET6_ADDRSTRLEN);
                if (client_ip == NULL) {
                    fprintf(stderr, "Failed to convert client IP address\n");
                    closesocket(client_socket);
                    continue;
                }
            } else if (client_addr.sin_family == AF_INET6) {
                struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&client_addr;
                const char *client_ip = inet_ntop(AF_INET6, &(ipv6->sin6_addr), clients[num_clients].ip, INET6_ADDRSTRLEN);
                if (client_ip == NULL) {
                    fprintf(stderr, "Failed to convert client IP address\n");
                    closesocket(client_socket);
                    continue;
                }
            } else {
                fprintf(stderr, "Unsupported address family\n");
                closesocket(client_socket);
                continue;
            }

            printf("\nClient '%s' connected from IP: %s\n", clients[num_clients].name, clients[num_clients].ip);
            char joinMes[] = "Connected clients: ";

            // Send the updated list of clients to all connected clients
            for (int j = 0; j < MAX_CLIENTS; j++) {
                if (clients[j].socket != -1) {
                    // Send joinMes to the newly connected client
                    send(clients[j].socket, joinMes, strlen(joinMes), 0);
                    // Send each client's name to all connected clients
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        send(clients[j].socket, clients[i].name, strlen(clients[i].name), 0);
                        send(clients[j].socket, "\t", strlen("\t"), 0);
                    }
                }
            }

            // Create a thread for the client
            if (pthread_create(&client_threads[num_clients], NULL, handle_client, (void *)&clients[num_clients]) != 0) {
                fprintf(stderr, "Thread creation failed\n");
                closesocket(client_socket);
            } else {
                num_clients++;
            }
        } else {
            fprintf(stderr, "Maximum clients reached. Connection rejected.\n");
            closesocket(client_socket);
        }
    }

    // Clean up
    closesocket(server_socket);
    WSACleanup();

    return 0;
}

// Function to handle client communication
void *handle_client(void *arg) {
    struct Client *client = (struct Client *)arg;
    int client_socket = client->socket;

    while (1) {
        char buffer[256];
        ssize_t num_bytes = recv(client_socket, buffer, sizeof(buffer), 0);

        if (num_bytes <= 0) {
            // Client disconnected
            printf("\nClient '%s' disconnected\n", client->name);
            break;
        }

        buffer[num_bytes] = '\0';



        char recipient_name[256];
        char sender_name [50];

        for (int k = 0; k < MAX_CLIENTS; k++) {
            if (client_socket == clients[k].socket) {
                strcpy(sender_name,clients[k].name);
                strcat(sender_name," : ");
                break;
            }
        }

        // Check if the message is a private message
        if (buffer[0] == '@') {
            // Find the recipient's name
            int i = 1;
            while (buffer[i] != ' ' && buffer[i] != '\0') {
                recipient_name[i - 1] = buffer[i];
                i++;
            }
            recipient_name[i - 1] = '\0';

            // Find the recipient's socket
            int recipient_socket = -1;
            for (int j = 0; j < MAX_CLIENTS; j++) {
                if (strcmp(recipient_name, clients[j].name) == 0) {
                    recipient_socket = clients[j].socket;
                    break;
                }
            }

            if (recipient_socket != -1) {
                // Send the private message to the recipient
                char *message = buffer + i + 1;  // Skip '@recipient_name '
                if(rndCorrupt(  client->name , message ) != -1){
                    char newmessage[250];
                    extractMessageWithoutParity(message,newmessage);
                    char fullmessage[256] = "private-> " ;
                    strcat(fullmessage,sender_name);
                    strcat(fullmessage ,newmessage);
                    ssize_t fullmessage_length = strlen(fullmessage);


                    // Perform the parity check
                    cmpParity(message,extractParity(message));
                    send(recipient_socket, fullmessage, fullmessage_length, 0);
                    printf("\n  %s sent a message to %s \" %s \"  ",sender_name,recipient_name,newmessage);
                }

            } else {
                // Recipient not found, send an error message back to the sender
                const char *error_message = "Recipient not found\n";
                send(client_socket, error_message, strlen(error_message), 0);
            }
        } else  {
            char pumessage[256] ;
            char nonparitymessage[256] ;
            strcpy(pumessage,"Public-> ");
            strcat(pumessage,sender_name);
            strcat(pumessage ,buffer);
            ssize_t pumessage_length = strlen(pumessage);
            rndCorrupt(  client->name , buffer );

            extractMessageWithoutParity(buffer,  nonparitymessage);
            printf("\n public -> %s sent a public message : \" %s \"  ",sender_name,nonparitymessage);
            cmpParity(nonparitymessage,extractParity(buffer));

            // Broadcast the message to all clients (excluding the sender)
            for (int j = 0; j < MAX_CLIENTS; j++) {
                if (clients[j].socket != -1 && clients[j].socket != client_socket) {
                    send(clients[j].socket,pumessage , pumessage_length, 0);
                }
            }
        }
    }

    // Clean up
    closesocket(client_socket);
    pthread_exit(NULL);
}

// Function to calculate parity of a message
int calculateParity(const char *message) {
    int parity = 0;

    // Iterate through each character in the message
    while (*message != '\0') {
        for (int i = 0; i < 8; i++) {
            if (*message & (1 << i)) {
                parity = !parity;
            }
        }
        message++;
    }

    return parity;
}

// Function to extract parity from a message
int extractParity(const char *message) {
    int length = strlen(message);
    if (length > 0) {
        char parityChar = message[length - 1];
        int parity = parityChar - '0';
        return parity;
    }
    return -1; // Error: invalid message length
}
// Function to extract message without parity from input
void extractMessageWithoutParity(const char *input, char *output) {
    int length = strlen(input);

    // Check if the input has at least one character
    if (length > 1) {
        // Copy all characters except the last one to the output
        strncpy(output, input, length - 1);
        // Ensure proper null-termination of the output string
        output[length - 1] = '\0';
    } else {
        // If input has 1 or 0 characters, set output to an empty string
        output[0] = '\0'; // Empty output string for invalid input
    }
}

// Function to compare parity of a message with a provided parity value
void cmpParity(const char* message, int parity) {
    // Call calculateParity function (assumed to be defined elsewhere)
    if (calculateParity(message) == parity) {
        // If parity matches, print a message indicating no error
        printf("\nNo error (Checked by Parity) \n");
    } else {
        // If parity mismatch, print an error message
        printf("\nError (Checked by Parity)\n");
}
}
int rndCorrupt( char clientname[] ,char* buffer ) {
// Randomly decide whether to corrupt the message or not
    ssize_t num_bytes = strlen(buffer);
    int should_corrupt = rand() % 2; // 0 or 1

    if (should_corrupt) {
// Corrupt the message
        for (int i = 0; i < num_bytes; i++) {
            int random_bit = rand() % 8; // 0 to 7
            buffer[i] ^= (1 << random_bit); // Flip the random bit
        }
        printf("\nMessage from client '%s' was corrupted\n", clientname);
        return -1;
    }
    return 0;
}