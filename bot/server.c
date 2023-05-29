#include <stdio.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/select.h>
#include <stdarg.h>
#include <time.h>

#define CONFIG_FILENAME "config"
#define LOG_FILENAME "/tmp/server.log"
#define MAX_CONNECTIONS 100
#define MAX_CLIENTS 10000
#define BUF_SIZE 32

void write_log(char *format, ...) {
    FILE* log_file = fopen(LOG_FILENAME, "a");
    if (log_file == NULL) {
        perror("Error while opening log file");
        exit(EXIT_FAILURE);
    }

    va_list arg_list;
    va_start(arg_list, format);

    time_t now;
    time(&now);
    struct tm *local = localtime(&now);

    fprintf(log_file, "%02d:%02d:%02d ", local->tm_hour, local->tm_min, local->tm_sec);
    vfprintf(log_file, format, arg_list);
    vfprintf(log_file, "\n", NULL);

    va_end(arg_list);
    fflush(log_file);

    fclose(log_file);
}

int create_socket(char* socket_name) {
    int fd;
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("Error while creating socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un address;
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, socket_name);

    unlink(socket_name);
    if (bind(fd, (struct sockaddr*) &address, sizeof(address)) < 0) {
        perror("Error while binding socket");
        exit(EXIT_FAILURE);
    }

    if (listen(fd, MAX_CONNECTIONS) < 0) {
        perror("Error while listening");
        exit(EXIT_FAILURE);
    }

    return fd;
}

int main() {

    FILE* config = fopen(CONFIG_FILENAME, "r");
    if (config == NULL) {
        perror("Error while opening config file");
        exit(EXIT_FAILURE);
    }

    char buf[BUF_SIZE];
    while (fgets(buf, BUF_SIZE, config) != NULL) {
        break;
    }

    int server = create_socket(buf);
    int client, clients[MAX_CLIENTS];
    char buffers[MAX_CLIENTS][BUF_SIZE];
    struct sockaddr client_addr;
    socklen_t len = sizeof(struct sockaddr);
    int state = 0;

    struct timeval timeout;
    timeout.tv_sec = 1;

    fd_set r, w;
    FD_ZERO(&r);
    FD_ZERO(&w);
    FD_SET(server, &r);
    int max_fd = server;

    int cnt = 0;
    while (1) {
        fd_set temp_r = r, temp_w = w;

        if (select(max_fd + 1, &temp_r, &temp_w, NULL, &timeout) < 0) {
            perror("Error while executing `select`");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(server, &temp_r)) {
            if ((client = accept(server, &client_addr, &len)) < 0) {
                perror("Error while accepting");
                return 1;
            }

            write_log("Client %d connected, fd = %d, sbrk = %#010x", cnt, client, sbrk(0));

            FD_SET(client, &r);
            clients[cnt] = client;
            memset(buffers[cnt], 0, BUF_SIZE);
            cnt++;

            max_fd = (client > max_fd) ? client : max_fd;
        }

        for (int i = 0; i < cnt; i++) {
            int fd = clients[i];

            if (!FD_ISSET(fd, &temp_r)) {
                continue;
            }

            int n = recv(fd, buf, BUF_SIZE, 0);
            if (n < 0) {
                perror("Error while receiving");
                exit(EXIT_FAILURE);
            } else if (n == 0) {
                FD_CLR(fd, &r);
                clients[i] = 0;
                write_log("Client %d disconnected", i);
                close(fd);
            } else {
                for (int j = 0; j < n; j++) {
                    if (buf[j] == '\n') {
                        state += atoi(buffers[i]);
                        memset(buffers[i], 0, BUF_SIZE);
                        FD_SET(fd, &w);
                    } else {
                        char* ptr = buffers[i] + strlen(buffers[i]);
                        *ptr = buf[j];
                    }
                }

                write_log("Received data from client %d: %.*s", i, n, buf);
            }
        }

        for (int i = 0; i < cnt; i++) {
            int fd = clients[i];

            if (!FD_ISSET(fd, &temp_r)) {
                continue;
            }

            write_log("Send data to client %d: %d", i, state);

            int n = snprintf(buf, BUF_SIZE, "%d\n", state);

            write(fd, buf, n);

            FD_CLR(fd, &w);
        }
    }

    return 0;
}