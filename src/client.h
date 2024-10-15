#ifndef POMOBAR_CLIENT_H
#define POMOBAR_CLIENT_H

void send_command(int sock_fd, const char* command);
void receive_response(int sock_fd);

#endif