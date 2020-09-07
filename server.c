//
// Created by William on 29/08/2020.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <wait.h>
#include "server.h"

#define PORT 9122
#define BACKLOG 5
#define BUF_SIZE 1025
#define MAX_CLIENTS 30

int main() {

    fd_set readset;
    char buffer[BUF_SIZE];
    int client_socket[MAX_CLIENTS];
    int maxClients = MAX_CLIENTS, maxSd, valread;

    int *sharedMemory = allocateSharedMemory(sizeof(client_socket));

    for (int i=0;i<maxClients;i++) {
        client_socket[i] = 0;
        sharedMemory[i] = 0;
    }


    int masterSocket = createSocket();

    struct sockaddr_in serverAddress = setServerAddress(PORT);

    bindServerSocket(masterSocket, serverAddress);

    startListening(masterSocket, BACKLOG);

    int addrlen = sizeof(serverAddress);

    while (1) {
        FD_ZERO(&readset);
        int sd, newSocket;

        FD_SET(masterSocket, &readset);
        maxSd = masterSocket;

        for (int i=0;i<maxClients;i++) {
            sd = client_socket[i];
            if (sd > 0) {
                FD_SET(sd, &readset);
            }
            if (sd > maxSd) {
                maxSd = sd;
            }
        }

        int activity = select(maxSd+1, &readset, NULL, NULL, NULL);

        if (FD_ISSET(masterSocket, &readset)) {
            handleNewConnection(masterSocket, serverAddress, &addrlen, client_socket, maxClients);
        }

        for (int i=0;i<maxClients;i++) {
            sd = client_socket[i];

            if (FD_ISSET(sd, &readset)) {
                if (sharedMemory[i] == 1) {
                    continue;
                }
                if ((valread = read(sd, buffer, BUF_SIZE)) <= 0) {
                    // Disconnection
                    handleDisconnection(sd, serverAddress, &addrlen, client_socket, i);
                }

                else {
                    // Handle input
                    if (strcmp(buffer, "keepalive") == 0) {
                        char stillAlive[BUF_SIZE];
                        strcpy(stillAlive, "stillalive");
                        if (send(sd, stillAlive, sizeof(char)*BUF_SIZE, 0) == -1) {
                            handleDisconnection(sd, serverAddress, &addrlen, client_socket, i);
                        }
                    }
                    else if(buffer[0] == 'p' && buffer[1] == 'u' && buffer[2] == 't') {
                        char *token = strtok(buffer, " ");
                        token = strtok(NULL, " ");
                        int force = atoi(token);
                        token = strtok(NULL, " ");
                        char progname[BUF_SIZE];
                        strcpy(progname, token);
                        token = strtok(NULL, " ");
                        int files = atoi(token);
                        sharedMemory[i] = 1;
                        put(sd, force, progname, files, sharedMemory, i);
                    }
                    else if (buffer[0] == 'g' && buffer[1] == 'e' && buffer[2] == 't') {
                        char *token = strtok(buffer, " ");
                        token = strtok(NULL, " ");
                        if (token == NULL) {
                            send(sd, "Missing progname, please use format get <progname> <sourcefile>.\n", sizeof(char)*BUF_SIZE, 0);
                            continue;
                        }
                        char progname[BUF_SIZE];
                        strcpy(progname, token);
                        token = strtok(NULL, " ");
                        if (token == NULL) {
                            send(sd, "Missing sourcefile, please use format get <progname> <sourcefile>.\n", sizeof(char)*BUF_SIZE, 0);
                            continue;
                        }
                        char filename[BUF_SIZE];
                        strcpy(filename, token);
                        sharedMemory[i] = 1;
                        get(sd, progname, filename, sharedMemory, i);
                    }
                    else if (buffer[0] == 'r' && buffer[1] == 'u' && buffer[2] == 'n') {
                        sharedMemory[i] = 1;
                        run(sd, buffer, sharedMemory, i);
                    }
                    else if (buffer[0] == 'l' && buffer[1] == 'i' && buffer[2] == 's' && buffer[3] == 't') {
                        list(buffer, sd);
                    }
                    else if (strcmp(buffer, "shutdown") == 0) {
                        char shutdown[BUF_SIZE];
                        strcpy(shutdown, "Shutdown command received, server is shutting down.\n");
                        printf("%s\n", shutdown);
                        for (int j=0;j<maxClients;j++) {
                            if (client_socket[j] == 0) {
                                continue;
                            }
                            send(client_socket[j], shutdown, sizeof(char)*BUF_SIZE, 0);
                        }
                        sleep(1);
                        exit(0);
                    }
                    else if (strcmp(buffer, "sys") == 0) {
                        sys(buffer, sd);
                    }
                    else {
                        strcpy(buffer, "Unknown command, please try again.\n");
                        send(sd, buffer, sizeof(char)*BUF_SIZE, 0);
                    }
                }
            }
        }

    }
    return 0;
}
