#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define CONFIG_FILENAME "config"

int msleep(long msec) {
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

int main(int argc, char* argv[]) {
    int sleep_time;
    if (argc < 2) {
        sleep_time = 200;
    } else {
        sleep_time = atoi(argv[1]);
    }

    FILE* config = fopen(CONFIG_FILENAME, "r");
    if (config == NULL) {
        perror("Error while opening config file");
        exit(EXIT_FAILURE);
    }

    char buf[32];
    while (fgets(buf, 32, config) != NULL) {
        break;
    }

    int server;
    if ((server = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("Error while creating socket");
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_un address;
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, buf);
    
    struct timeval timeout;
    timeout.tv_sec = 1;
    setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);

    if (connect(server, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("Error while connect");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));
    int random = rand() % 256;

    int cnt = 0;
    char c;
    int sum_sleep_time = 0;
    while (fread(&c, 1, 1, stdin)) {
        cnt++;
        if (cnt % 256 == random) {
            msleep(sleep_time);
            sum_sleep_time += sleep_time;
            random = rand() % 256;
        }

        if (send(server, &c, 1, 0) < 0) {
            perror("Error while sending data");
            exit(EXIT_FAILURE);
        }

        if (c == '\n') {
            recv(server, buf, 32, 0);
        }
    }

    recv(server, buf, 32, 0);

    printf("%d\n", sum_sleep_time);
    shutdown(server, SHUT_RDWR);
    close(server);

    return 0;
}