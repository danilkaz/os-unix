#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/prctl.h>

#define LOG_FILENAME "/tmp/myinit.log"

#define MAX_PROCS 16
#define MAX_ARGS 16        
#define MAX_LINE_LENGTH 1024

FILE* log_file;
char *config_filename;

struct Subprocess {
    int argc;
    char **argv;
    char *in;
    char *out;
};

struct Subprocess **subprocs;
pid_t pids[MAX_PROCS];

void write_log(char *format, ...) {
    va_list arg_list;
    va_start(arg_list, format);
    vfprintf(log_file, format, arg_list);
    vfprintf(log_file, "\n", NULL);
    va_end(arg_list);
    fflush(log_file);
}

int exec_command(struct Subprocess *sp) {
    pid_t pid = fork();

    switch (pid) {
    case -1:
        perror("Error while creating fork");
        return -1;
    case 0:
        int fd_in = open(sp->in, O_RDONLY);
        if (fd_in < 0) {
            perror("Error while opening `in` file");
            exit(EXIT_FAILURE);
        }
        dup2(fd_in, STDIN_FILENO);

        int fd_out = open(sp->out, O_WRONLY | O_CREAT);
        if (fd_out < 0) {
            perror("Error while opening `out` file");
            exit(EXIT_FAILURE);
        }
        dup2(fd_out, STDOUT_FILENO);

        if (execv(sp->argv[0], sp->argv) == -1) {
            perror("Error while exec command");
            exit(EXIT_FAILURE);
        }

        return getpid();
    default:
        return pid;
    }
}

int is_absolute(char *path) {
    return strlen(path) >= 1 && path[0] == '/';
}

int read_config_file() {
    FILE* file = fopen(config_filename, "r");
    if (file == NULL) {
        perror("Error while opening config file");
        return -1;
    }

    int index = 0;
    char buf[MAX_LINE_LENGTH];
    while (fgets(buf, sizeof(buf), file) != NULL) {
        buf[strcspn(buf, "\n")] = 0;

        char **line_args = malloc(sizeof(char*) * MAX_ARGS);
        char *line_arg = strtok(buf, " ");
        int line_args_count = 0;
        while (line_arg != NULL) {
            line_args[line_args_count] = malloc(sizeof(char) * (strlen(line_arg) + 1));
            strcpy(line_args[line_args_count], line_arg);
            line_args_count++;
            line_arg = strtok(NULL, " ");
        }

        if (line_args_count < 3) {
            fprintf(stderr, "Too few arguments in line %d\n", index);
            continue;
        }

        if (!is_absolute(line_args[0]) || !is_absolute(line_args[line_args_count - 2]) || !is_absolute(line_args[line_args_count - 1])) {
            fprintf(stderr, "Path is not absolute");
            continue;
        }

        char **sp_argv = malloc(sizeof(char*) * (line_args_count - 2));
        int sp_argc = 0;

        for (int i = 0; i < line_args_count - 2; i++) {
            sp_argv[sp_argc] = malloc(sizeof(char) * (strlen(line_args[i]) + 1));
            strcpy(sp_argv[sp_argc], line_args[i]);
            sp_argc++;
        }

        subprocs[index] = (struct Subprocess*)malloc(sizeof(struct Subprocess));

        subprocs[index]->argc = sp_argc;
        subprocs[index]->argv = sp_argv;
        subprocs[index]->in = line_args[line_args_count - 2];
        subprocs[index]->out = line_args[line_args_count - 1];

        pids[index] = exec_command(subprocs[index]);
        if (pids[index] < 0) {
            continue;
        }

        write_log("Process %d started", pids[index]);

        index++;
    }

    for (int i = index; i < MAX_PROCS; i++) {
        subprocs[i] = NULL;
    }

    fclose(file);
    return index;
}

void handle_sighup(int sig) {
    write_log("Received SIGHUP, restarting processes");

    for (int i = 0; i < MAX_PROCS; i++) {
        if (pids[i] > 0) {
            kill(pids[i], SIGTERM);
        }
    }

    run_init();
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <config>", argv[0]);
        exit(EXIT_FAILURE);
    }

    config_filename = argv[1];

    if (chdir("/") != 0) {
        perror("Error while changing directory to root");
        exit(EXIT_FAILURE);
    }

    for (int i = getdtablesize() - 1; i >= 0; i--) {
        close(i);
    }

    if (fork() != 0) {
        exit(0);
    }

    if (setsid() == -1) {
        perror("Error while creating new session");
        exit(EXIT_FAILURE);
    }
 
    log_file = fopen(LOG_FILENAME, "a");
    if (log_file == NULL) {
        perror("Error while opening log file");
        exit(EXIT_FAILURE);
    }

    signal(SIGHUP, handle_sighup);

    subprocs = malloc(sizeof(struct Subprocess *) * MAX_PROCS);

    run_init();

    return 0;
}

void run_init() {
    if (read_config_file() <= 0) {
        perror("Error while reading config file");
        exit(EXIT_FAILURE);
    }

    int status;
    while (1) {
        pid_t pid = wait(&status);

        for (int i = 0; i < MAX_PROCS; i++) {
            if (pids[i] <= 0) {
                continue;
            }

            if (pids[i] == pid || errno == ECHILD) {
                int old_pid = pids[i];
                write_log("Process %d finished with status %d", old_pid, status);

                if (subprocs[i] != NULL) {
                    write_log("Restarting process %d", old_pid);
                    pids[i] = exec_command(subprocs[i]);
                    write_log("Process %d restarted with pid %d", old_pid, pids[i]);
                }
            }
        }
    }
}