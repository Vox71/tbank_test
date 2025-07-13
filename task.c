#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return EXIT_FAILURE;
    }

    if (pid > 0) {
        FILE *file = fopen(argv[1], "w");
        if (file == NULL) {
            perror("Error opening file for writing");
            return EXIT_FAILURE;
        }
        if (fprintf(file, "%s", "Hello, World!\n") < 0) {
            perror("Error writing to file");
            fclose(file);
            return EXIT_FAILURE;
        }
        if (fclose(file) != 0) {
            perror("Error closing file");
            return EXIT_FAILURE;
        }

        wait(NULL);
    } else {
        FILE *passwd_file = fopen("/usr/passwd", "r");
        if (passwd_file == NULL) {
            perror("Error opening /usr/passwd");
            exit(EXIT_FAILURE);
        }
        printf("Contents of /usr/passwd:\n");
        int ch;
        while ((ch = fgetc(passwd_file)) != EOF) {
            putchar(ch);
        }
        fclose(passwd_file);
    }

    return EXIT_SUCCESS;
}