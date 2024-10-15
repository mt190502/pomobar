#ifndef POMOBAR_SERVER_H
#define POMOBAR_SERVER_H

char* calculate_time(int time);
void* pomobar_thread_func(void* arg);
void handle_client(int client_fd);
void send_status(int client_fd);
void send_message(int client_fd, const char* message);

#endif