#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define MAX_PID_LEN 10

char* stats_filename = "stats.txt";

int stop = 0;

int successes = 0;
int failures = 0;
char filename[256];
char lock_filename[256];

void create_lock_file() {
    int lock_fd = open(lock_filename, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (lock_fd < 0) {
        if (errno == EEXIST) {
            while (access(lock_filename, F_OK) == 0) {
                sleep(1);
            }
            create_lock_file();
        } else {
            perror("Cannot create lock file");
            exit(EXIT_FAILURE);
        }
    } else {
        char pid_str[MAX_PID_LEN];
        int pid_str_len = sprintf(pid_str, "%d", getpid());
        if (write(lock_fd, pid_str, pid_str_len) < 0) {
            perror("Cannot write to lock file");
            exit(EXIT_FAILURE);
        }

        if (close(lock_fd) < 0) {
            perror("Cannot close lock file");
            exit(EXIT_FAILURE);
        }
    }
}

void record_statistics() {
    FILE* stats_file = fopen(stats_filename, "a");
    if (stats_file != NULL) {
        fprintf(stats_file, "%d - Successes: %d, Failures: %d\n", getpid(), successes, failures);
        if (fclose(stats_file) < 0) {
            perror("Cannot close stats file");
            exit(EXIT_FAILURE);
        }
    } else {
        perror("Cannot open stats file");
        exit(EXIT_FAILURE);
    }
}

void handle_sigint(int sig) {
    stop = 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file> \n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }

    sprintf(filename, "%s", argv[1]);
    sprintf(lock_filename, "%s.lck", argv[1]);

    while (!stop) {
        create_lock_file();
        
        int lock_fd = open(lock_filename, O_RDONLY);
        if (lock_fd < 0) {
            perror("Cannot open lock file");
            exit(EXIT_FAILURE);
        }

        char lock_pid_str[MAX_PID_LEN];
        int lock_pid_str_len = read(lock_fd, lock_pid_str, MAX_PID_LEN);
        if (lock_pid_str_len < 0) {
            perror("Cannot read lock file");
            exit(EXIT_FAILURE);
        } else if (lock_pid_str_len == 0) {
            fprintf(stderr, "Lock file is empty\n");
            failures++;
            break;
        } else {
            int lock_pid = atoi(lock_pid_str);
            if (lock_pid != getpid()) {
                fprintf(stderr, "File is locked by another process %d\n", lock_pid);
                failures++;
                break;
            }
        }

        int fd = open(filename, O_WRONLY | O_APPEND);
        if (fd < 0) {
            perror("Cannot open my file");
            exit(EXIT_FAILURE);
        }

        sleep(1);
        close(fd);
        successes++;

        if (unlink(lock_filename) < 0) {
            perror("Cannot remove lock file");
            exit(EXIT_FAILURE);
        }

        sleep(1);
    }

    record_statistics();
}
