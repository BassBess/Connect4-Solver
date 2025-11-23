#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>

int network_create_server(int port);
int network_accept_client(int server_fd);
int network_connect_to_server(const char *ip, int port);
bool network_send_move(int socket_fd, int column);
int network_receive_move(int socket_fd);
bool network_send_first_player(int socket_fd, int first);
int network_receive_first_player(int socket_fd);
void network_close(int socket_fd);

#endif
