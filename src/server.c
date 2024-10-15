#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include "server.h"

// Socket settings
#define SOCK_NAME "pomobar.sock"
#define BUFFER_SIZE 512
#define MAX_CLIENTS 3

// Pomobar settings
#define POMOBAR_WORK_DURATION 1500        // 25 minutes
#define POMOBAR_SHORT_BREAK_DURATION 300  // 5 minutes
#define POMOBAR_LONG_BREAK_DURATION 900   // 15 minutes
#define POMOBAR_BEFORE_LONG_BREAK 4

// Pomobar states
typedef enum {
    POMOBAR_STATE_IDLE,
    POMOBAR_STATE_WORK,
    POMOBAR_STATE_SHORT_BREAK,
    POMOBAR_STATE_LONG_BREAK,
    POMOBAR_STATE_PAUSED
} pomobar_state_t;

// Pomobar values
typedef struct {
    pomobar_state_t state;
    pomobar_state_t state_before_pause;
    int pomodoro_count;
    int remaining_time;
    pthread_mutex_t lock;
} pomobar_t;

// Global variables
pomobar_t instance;
int server_running = 1;
int client_is_non_interactive = 0;

int main(void) {
    //* define socket variables
    int server_fd, client_fd;
    struct sockaddr_un server_addr, client_addr;

    //* get user id
    uid_t uid = getuid();

    //* setup instance
    instance.state = POMOBAR_STATE_IDLE;
    instance.pomodoro_count = 0;
    instance.remaining_time = 0;
    pthread_mutex_init(&instance.lock, NULL);

    //* create socket
    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("error: unix socket creation error");
        exit(1);
    }

    //* setup socket address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    if (uid == 0) {
        strncpy(server_addr.sun_path, "/tmp/pomobar-0.sock", sizeof(server_addr.sun_path) - 1);
    } else {
        char sock_path[40];
        snprintf(sock_path, sizeof(sock_path), "/run/user/%d/%s", uid, SOCK_NAME);
        strncpy(server_addr.sun_path, sock_path, sizeof(server_addr.sun_path) - 1);
    }
    unlink(server_addr.sun_path);

    //* bind socket
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("error: unix socket bind error");
        exit(1);
    }

    //* listen for clients
    if (listen(server_fd, MAX_CLIENTS) == -1) {
        perror("error: unix socket listen error");
        exit(1);
    }
    printf("info: pomobar server started at %s\n", server_addr.sun_path);

    //* create pomobar thread
    pthread_t pomobar_thread;
    if (pthread_create(&pomobar_thread, NULL, pomobar_thread_func, NULL) != 0) {
        perror("error: pomodoro thread creation error");
        close(server_fd);
        exit(1);
    }

    //* accept clients
    while (server_running) {
        if ((client_fd = accept(server_fd, NULL, NULL)) == -1) {
            perror("error: unix socket accept error");
            continue;
        }
        handle_client(client_fd);
        close(client_fd);
    }

    //* cleanup
    close(server_fd);
    unlink(server_addr.sun_path);
    pthread_mutex_destroy(&instance.lock);
    return 0;
}

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(client_fd, buffer, BUFFER_SIZE)) > 0) {
        buffer[bytes_read] = '\0';

        char* command = strtok(buffer, "\n\r ");
        if (strncmp(command, "non-interactive/", 16) == 0) {
            client_is_non_interactive = 1;
            command += 16;
        } else {
            client_is_non_interactive = 0;
        }

        if (command == NULL) {
            send_message(client_fd, "error: invalid command");
            continue;
        }

        // TODO: Create notification for each state
        if (strcmp(command, "start") == 0) {
            pthread_mutex_lock(&instance.lock);
            if (instance.state == POMOBAR_STATE_IDLE) {
                instance.state = POMOBAR_STATE_WORK;
                instance.remaining_time = POMOBAR_WORK_DURATION;

                send_message(client_fd, "info: pomodoro started");
            } else {
                send_message(client_fd, "info: pomodoro already running");
            }
            pthread_mutex_unlock(&instance.lock);
        } else if (strcmp(command, "pause") == 0) {
            pthread_mutex_lock(&instance.lock);
            if (instance.state == POMOBAR_STATE_IDLE) {
                instance.state = POMOBAR_STATE_WORK;
                instance.remaining_time = POMOBAR_WORK_DURATION;
                send_message(client_fd, "info: pomodoro started because it wasn't running");
            } else if (instance.state == POMOBAR_STATE_PAUSED) {
                instance.state = instance.state_before_pause;
                send_message(client_fd, "info: pomodoro resumed from pause");
            } else {
                instance.state_before_pause = instance.state;
                instance.state = POMOBAR_STATE_PAUSED;
                send_message(client_fd, "info: pomodoro paused");
            }
            pthread_mutex_unlock(&instance.lock);
        } else if (strcmp(command, "resume") == 0) {
            pthread_mutex_lock(&instance.lock);
            if (instance.state == POMOBAR_STATE_PAUSED) {
                instance.state = instance.state_before_pause;
                send_message(client_fd, "info: pomodoro resumed");
            } else {
                send_message(client_fd, "error: pomodoro not paused");
            }
            pthread_mutex_unlock(&instance.lock);
        } else if (strcmp(command, "reset") == 0) {
            pthread_mutex_lock(&instance.lock);
            instance.state = POMOBAR_STATE_IDLE;
            instance.pomodoro_count = 0;
            instance.remaining_time = 0;
            send_message(client_fd, "info: pomodoro reset");
            pthread_mutex_unlock(&instance.lock);
        } else if (strcmp(command, "status") == 0) {
            send_status(client_fd);
        } else {
            char error_message[BUFFER_SIZE];
            snprintf(error_message, BUFFER_SIZE, "error: invalid command (%s)", command);
            send_message(client_fd, error_message);
        }
    }

    if (bytes_read == -1) {
        perror("error: unix socket read error");
    }
}

void send_message(int client_fd, const char* message) {
    char formatted_message[BUFFER_SIZE];
    if (client_is_non_interactive) {
        snprintf(formatted_message, BUFFER_SIZE, "{\"text\":\"%s\"}", message);
    } else {
        snprintf(formatted_message, BUFFER_SIZE, "pomobar server> %s", message);
    }
    write(client_fd, formatted_message, strlen(formatted_message));
}

void send_status(int client_fd) {
    char buffer[BUFFER_SIZE];
    pthread_mutex_lock(&instance.lock);
    char* calculated_remaining_time = calculate_time(instance.remaining_time);
    char* calculated_work_time_duration = calculate_time(POMOBAR_WORK_DURATION);
    char* calculated_short_break_time_duration = calculate_time(POMOBAR_SHORT_BREAK_DURATION);
    char* calculated_long_break_time_duration = calculate_time(POMOBAR_LONG_BREAK_DURATION);
    char state_text[100], state_tooltip[BUFFER_SIZE], state_alt[100], state_icon[4];

    switch (instance.state) {
        case POMOBAR_STATE_IDLE:
            strcpy(state_icon, "");
            strcpy(state_text, "Pomobar");
            strcpy(state_alt, "Idle");
            sprintf(state_tooltip,
                    "Status: Idle\\nPomodoro count: %d\\n\\nTargeted pomodoro count: %d\\nTargeted work "
                    "duration: %s\\nTargeted short break duration: %s\\nTargeted long break duration: %s",
                    instance.pomodoro_count, POMOBAR_BEFORE_LONG_BREAK, calculated_work_time_duration,
                    calculated_short_break_time_duration, calculated_long_break_time_duration);
            break;
        case POMOBAR_STATE_WORK:
            strcpy(state_icon, "");
            sprintf(state_text, "%s/%s", calculated_remaining_time, calculate_time(POMOBAR_WORK_DURATION));
            strcpy(state_alt, "Work");
            sprintf(state_tooltip,
                    "Status: Work\\nRemaining time: %s\\nPomodoro count: %d\\n\\nTargeted pomodoro count: %d\\n"
                    "Targeted work duration: %s\\nTargeted short break duration: %s\\nTargeted long break duration: %s",
                    calculated_remaining_time, instance.pomodoro_count, POMOBAR_BEFORE_LONG_BREAK,
                    calculated_work_time_duration, calculated_short_break_time_duration,
                    calculated_long_break_time_duration);
            break;
        case POMOBAR_STATE_SHORT_BREAK:
            strcpy(state_icon, "");
            sprintf(state_text, "%s/%s", calculated_remaining_time, calculate_time(POMOBAR_SHORT_BREAK_DURATION));
            strcpy(state_alt, "Short Break");
            sprintf(state_tooltip,
                    "Status: Short Break\\nRemaining time: %s\\nPomodoro count: %d\\n\\nTargeted pomodoro "
                    "count: %d\\nTargeted work duration: %s\\nTargeted short break duration: %s\\nTargeted long "
                    "break duration: %s",
                    calculated_remaining_time, instance.pomodoro_count, POMOBAR_BEFORE_LONG_BREAK,
                    calculated_work_time_duration, calculated_short_break_time_duration,
                    calculated_long_break_time_duration);
            break;
        case POMOBAR_STATE_LONG_BREAK:
            strcpy(state_icon, "");
            sprintf(state_text, "%s/%s", calculated_remaining_time, calculate_time(POMOBAR_LONG_BREAK_DURATION));
            strcpy(state_alt, "Long Break");
            sprintf(state_tooltip,
                    "Status: Long Break\\nRemaining time: %s\\nPomodoro count: %d\\n\\nTargeted pomodoro count: "
                    "%d\\nTargeted work duration: %s\\nTargeted short break duration: %s\\nTargeted long break "
                    "duration: %s",
                    calculated_remaining_time, instance.pomodoro_count, POMOBAR_BEFORE_LONG_BREAK,
                    calculated_work_time_duration, calculated_short_break_time_duration,
                    calculated_long_break_time_duration);
            break;
        case POMOBAR_STATE_PAUSED:
            strcpy(state_icon, "");
            strcpy(state_alt, "Paused");
            if (instance.state_before_pause == POMOBAR_STATE_WORK) {
                sprintf(state_text, "%s/%s", calculated_remaining_time, calculate_time(POMOBAR_WORK_DURATION));
            } else if (instance.state_before_pause == POMOBAR_STATE_SHORT_BREAK) {
                sprintf(state_text, "%s/%s", calculated_remaining_time, calculate_time(POMOBAR_SHORT_BREAK_DURATION));
            } else if (instance.state_before_pause == POMOBAR_STATE_LONG_BREAK) {
                sprintf(state_text, "%s/%s", calculated_remaining_time, calculate_time(POMOBAR_LONG_BREAK_DURATION));
            }
            sprintf(state_tooltip,
                    "Status: Paused\\nRemaining time: %s\\nPomodoro count: %d\\n\\nTargeted pomodoro count: %d\\n"
                    "Targeted work duration: %s\\nTargeted short break duration: %s\\nTargeted long break duration: %s",
                    calculated_remaining_time, instance.pomodoro_count, POMOBAR_BEFORE_LONG_BREAK,
                    calculated_work_time_duration, calculated_short_break_time_duration,
                    calculated_long_break_time_duration);
            break;
    }
    // printf("===\n%s\n===", state_icon);
    if (client_is_non_interactive) {
        snprintf(buffer, BUFFER_SIZE, "{\"text\":\"%s %s\",\"alt\":\"%s\",\"tooltip\":\"%s\"}", state_icon, state_text,
                 state_alt, state_tooltip);
    } else {
        snprintf(buffer, BUFFER_SIZE,
                 "info: state: %s\nremaining time: %s\npomodoro count: %d\n\ntargeted pomodoro count: %d\ntargeted "
                 "work duration: %s\ntargeted short break duration: %s\ntargeted long break duration: %s\n",
                 state_text, calculated_remaining_time, instance.pomodoro_count, POMOBAR_BEFORE_LONG_BREAK,
                 calculated_work_time_duration, calculated_short_break_time_duration,
                 calculated_long_break_time_duration);
    }
    write(client_fd, buffer, strlen(buffer));
    pthread_mutex_unlock(&instance.lock);
}

char* calculate_time(int time) {
    char* buffer = malloc(10 * sizeof(char));
    if (buffer == NULL) {
        perror("error: memory allocation error");
        exit(1);
    }
    int minutes = time / 60;
    int seconds = time % 60;
    snprintf(buffer, 10, "%02d:%02d", minutes, seconds);
    return buffer;
}

void* pomobar_thread_func(void* arg) {
    while (server_running) {
        pthread_mutex_lock(&instance.lock);
        if ((instance.state != POMOBAR_STATE_IDLE) && (instance.state != POMOBAR_STATE_PAUSED)) {
            if (--instance.remaining_time <= 0) {
                switch (instance.state) {
                    case POMOBAR_STATE_WORK:
                        if ((instance.pomodoro_count) && (instance.pomodoro_count % POMOBAR_BEFORE_LONG_BREAK == 0)) {
                            instance.state = POMOBAR_STATE_LONG_BREAK;
                            instance.remaining_time = POMOBAR_LONG_BREAK_DURATION;
                        } else {
                            instance.state = POMOBAR_STATE_SHORT_BREAK;
                            instance.remaining_time = POMOBAR_SHORT_BREAK_DURATION;
                        }
                        break;
                    case POMOBAR_STATE_SHORT_BREAK:
                    case POMOBAR_STATE_LONG_BREAK:
                        instance.state = POMOBAR_STATE_WORK;
                        instance.remaining_time = POMOBAR_WORK_DURATION;
                        instance.pomodoro_count++;
                        break;
                    default:
                        break;
                }
            }
        }
        pthread_mutex_unlock(&instance.lock);
        sleep(1);
    }
    return NULL;
}
