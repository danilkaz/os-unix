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

#define LOG_FILENAME "/tmp/myinit.log"
#define CONFIG_FILENAME "/tmp/config"

#define MAX_PROCS 16
#define MAX_ARGS 16        
#define MAX_LINE_LENGTH 128 

FILE* log_file;

struct Subprocess {
    char *command;
    char **args;
    int args_count;
    char *in;
    char *out;
};

struct Subprocess **subprocs;
pid_t pids[MAX_PROCS];

int procs_count;

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

        if (execvp(sp->command, sp->args) == -1) {
            perror("Error while exec command");
            exit(EXIT_FAILURE);
        }

        write_log("Process %d started", getpid());
        return getpid();
    default:
        return pid;
    }
}

int is_absolute(char *path) {
    return strlen(path) >= 1 && path[0] == '/';
}

int read_config_file() {
    FILE* file = fopen(CONFIG_FILENAME, "r");
    if (file == NULL) {
        perror("Error while opening config file");
        return -1;
    }

    int count = 0;
    char buf[MAX_LINE_LENGTH];
    while (fgets(buf, sizeof(buf), file) != NULL) {
        buf[strcspn(buf, "\n")] = 0;

        char **args = malloc(sizeof(char*) * MAX_ARGS);
        char *arg = strtok(buf, " ");
        int args_count = 0;
        while (arg != NULL) {
            args[args_count] = malloc(sizeof(char) * (strlen(arg) + 1));
            strcpy(args[args_count], arg);
            args_count++;
            arg = strtok(NULL, " ");
        }

        if (args_count < 3) {
            fprintf(stderr, "Too few arguments in line %d\n", count);
            continue;
        }

        if (!is_absolute(args[0]) || !is_absolute(args[args_count - 2]) || !is_absolute(args[args_count - 1])) {
            fprintf(stderr, "Path is not absolute");
            continue;
        }

        char **sp_args = malloc(sizeof(char*) * (args_count - 3));
        int sp_args_count = 0;

        for (int i = 1; i < args_count - 2; i++) {
            sp_args[sp_args_count] = malloc(sizeof(char) * (strlen(args[i]) + 1));
            strcpy(sp_args[sp_args_count], args[i]);
            sp_args_count++;
        }

        subprocs[count] = (struct Subprocess*)malloc(sizeof(struct Subprocess));

        subprocs[count]->command = args[0];
        subprocs[count]->args = sp_args;
        subprocs[count]->args_count = sp_args_count;
        subprocs[count]->in = args[args_count - 2];
        subprocs[count]->out = args[args_count - 1];

        pids[count] = exec_command(subprocs[count]);
        if (pids[count] < 0) {
            continue;
        }

        count++;
    }

    fclose(file);
    return count;
}

void handle_sighup(int sig) {
    write_log("Received SIGHUP, restarting processes");

    for (int i = 0; i < procs_count; i++) {
        kill(pids[i], SIGTERM);
    }

    run_init();
}

int main(int argc, char *argv[]) {
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
    procs_count = read_config_file();
    if (procs_count <= 0) {
        perror("Error while reading config file");
        exit(EXIT_FAILURE);
    }

    int status;
    while (1) {
        for (int i = 0; i < procs_count; i++) {
            if (pids[i] == -1) {
                continue;
            }

            if (pids[i] == wait(&status) || errno == ECHILD) {
                write_log("Process %d finished with exit status %d, restarting", pids[i], status);

                pids[i] = exec_command(subprocs[i]);
            }
        }
    }
}