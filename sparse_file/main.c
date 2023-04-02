#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>

void finish_with_error(char *buf, int src_fd, int dest_fd, char *msg, int exit_code) {
    perror(msg);
    free(buf);
    close(src_fd);
    close(dest_fd);
    exit(exit_code);
}

int main(int argc, char **argv) {
    int block_size = 4096;
    int src_fd, dest_fd;

    int b;
    while ((b = getopt(argc, argv, "b:")) != -1) {
        switch (b) {
            case 'b':
                sscanf(optarg, "%d", &block_size);        
            default:
                break;
        }
    }
    argc -= optind;
    argv += optind;

    char *dest;
    switch (argc) {
        case 1:
            src_fd = 0;
            dest = argv[0];
            break;
        case 2:
            if ((src_fd = open(argv[0], O_RDONLY)) < 0) {
                perror("Error while opening source file");
                exit(1);
            }
            dest = argv[1];
            break;
    }

    if ((dest_fd = open(dest, O_CREAT|O_RDWR, 0666)) < 0) {
        perror("Error while opening destination file");
        close(src_fd);
        exit(2);
    }

    char *buf = (char *)malloc(block_size);

    int r;
    int size = 0;
    while((r = read(src_fd, buf, block_size)) != 0) {
        if (r < 0) {
            finish_with_error(buf, src_fd, dest_fd, "Error while reading block", 3);
        }

        size += r;

        bool is_zero_block = true;
        for (int c = 0; c < r; c++) {
            if (buf[c] != 0) {
                is_zero_block = false;
                break;
            }
        }

        if (is_zero_block) {
            if (lseek(dest_fd, r, SEEK_CUR) == -1) {
                finish_with_error(buf, src_fd, dest_fd, "Error while seeking", 4);
            }
        } else {
            if (write(dest_fd, buf, r) == -1) {
                finish_with_error(buf, src_fd, dest_fd, "Error while writing block", 5);
            }
        }
    }

    if (ftruncate(dest_fd, size) == -1) {
        finish_with_error(buf, src_fd, dest_fd, "Error while truncating", 6);
    }

    free(buf);
    close(src_fd);
    close(dest_fd);
    return 0;
}