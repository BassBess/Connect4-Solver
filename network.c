#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 256

int network_create_server(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        return -1;
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        close(server_fd);
        return -1;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 1) < 0) {
        perror("Listen failed");
        close(server_fd);
        return -1;
    }
    
    return server_fd;
}

int network_accept_client(int server_fd) {
    int client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    
    printf("\nWaiting for opponent to connect...\n");
    
    if ((client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
        perror("Accept failed");
        return -1;
    }
    
    printf("Opponent connected from %s\n", inet_ntoa(address.sin_addr));
    return client_fd;
}

int network_connect_to_server(const char *ip, int port) {
    int sock_fd;
    struct sockaddr_in serv_addr;
    
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock_fd);
        return -1;
    }
    
    printf("\nConnecting to %s:%d...\n", ip, port);
    
    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock_fd);
        return -1;
    }
    
    printf("Connected to server!\n");
    return sock_fd;
}

bool network_send_move(int socket_fd, int column) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "MOVE %d\n", column);
    
    if (send(socket_fd, buffer, strlen(buffer), 0) < 0) {
        perror("Send failed");
        return false;
    }
    return true;
}

int network_receive_move(int socket_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    
    memset(buffer, 0, BUFFER_SIZE);
    bytes_read = recv(socket_fd, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printf("\nOpponent disconnected.\n");
        } else {
            perror("Receive failed");
        }
        return -1;
    }
    
    int column;
    if (sscanf(buffer, "MOVE %d", &column) == 1) {
        return column;
    }
    
    return -1;
}

bool network_send_first_player(int socket_fd, int first) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "FIRST %d\n", first);
    
    if (send(socket_fd, buffer, strlen(buffer), 0) < 0) {
        perror("Send failed");
        return false;
    }
    return true;
}

int network_receive_first_player(int socket_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    
    memset(buffer, 0, BUFFER_SIZE);
    bytes_read = recv(socket_fd, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_read <= 0) {
        return -1;
    }
    
    int first;
    if (sscanf(buffer, "FIRST %d", &first) == 1) {
        return first;
    }
    
    return -1;
}

void network_close(int socket_fd) {
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}
