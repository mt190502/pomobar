#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "client.h"

#define SOCK_NAME "pomobar.sock"
#define BUFFER_SIZE 512

int client_is_non_interactive = 0;

int main(int argc, char* argv[]) {
    //* define socket variables
    int sock_fd;
    struct sockaddr_un server_addr;

    //* get user id
    uid_t uid = getuid();

    //* get NON_INTERACTIVE environment variable
    char* non_interactive_env = getenv("NON_INTERACTIVE");
    if (non_interactive_env != NULL) {
        client_is_non_interactive = atoi(non_interactive_env);
    } else {
        client_is_non_interactive = 0;
    }

    //* create socket
    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("error: unix socket creation error");
        exit(1);
    }

    //* setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    if (uid == 0) {
        strncpy(server_addr.sun_path, "/tmp/pomobar-0.sock", sizeof(server_addr.sun_path) - 1);
    } else {
        char sock_path[40];
        snprintf(sock_path, sizeof(sock_path), "/run/user/%d/%s", uid, SOCK_NAME);
        strncpy(server_addr.sun_path, sock_path, sizeof(server_addr.sun_path) - 1);
    }

    //* connect to server
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("error: unix socket connect error");
        close(sock_fd);
        exit(1);
    }

    //* main loop
    char buffer[BUFFER_SIZE];
    while (1) {
        if (argc > 1) {
            client_is_non_interactive = 1;
            send_command(sock_fd, argv[1]);
            receive_response(sock_fd);
            break;
        }
        if (!client_is_non_interactive)
            printf("pomobar client> ");

        if (!fgets(buffer, BUFFER_SIZE, stdin)) {
            break;
        }
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strcmp(buffer, "exit") == 0) {
            break;
        }
        if (buffer[0] == '\0') {
            continue;
        }
        send_command(sock_fd, buffer);
        receive_response(sock_fd);
    }
    close(sock_fd);
    return 0;
}

void send_command(int sock_fd, const char* command) {
    //* send command to server
    if (client_is_non_interactive) {
        char non_interactive[BUFFER_SIZE];
        snprintf(non_interactive, BUFFER_SIZE, "non-interactive/%s", command);
        if (write(sock_fd, non_interactive, strlen(non_interactive)) == -1) {
            perror("error: unix socket write error");
            close(sock_fd);
            exit(1);
        }
    } else {
        if (write(sock_fd, command, strlen(command)) == -1) {
            perror("error: unix socket write error");
            close(sock_fd);
            exit(1);
        }
    }
}

void receive_response(int sock_fd) {
    //* receive response from server
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    bytes_read = read(sock_fd, buffer, BUFFER_SIZE);
    if (bytes_read == -1) {
        perror("error: unix socket read error");
        close(sock_fd);
        exit(1);
    }
    buffer[bytes_read] = '\0';
    printf("%s", buffer);
}