#define _GNU_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fanotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define MAX_BLOCKED_FILES 32

int main(int argc, char **argv) {
    int fan_fd;
    char buf[4096];
    struct fanotify_event_metadata *metadata;
    char *blocked_files[MAX_BLOCKED_FILES] = {0};
    int num_blocked = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--block") == 0 && i+1 < argc) {
            if (num_blocked < MAX_BLOCKED_FILES) {
                blocked_files[num_blocked++] = argv[++i];
            }
        }
    }
    if (num_blocked == 0) {
        fprintf(stderr, "Usage: --block <file>\n");
        exit(EXIT_FAILURE);
    }

    fan_fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_CONTENT, O_RDONLY);
    if (fan_fd == -1) {
        perror("fanotify_init");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_blocked; i++) {
        if (fanotify_mark(fan_fd, 
                          FAN_MARK_ADD, 
                          FAN_OPEN_PERM, 
                          AT_FDCWD, 
                          blocked_files[i]) == -1) {
            perror("fanotify_mark");
            fprintf(stderr, "Failed to mark: %s\n", blocked_files[i]);
        }
    }

    printf("Blocking access to %d files for 'task' processes.\n", num_blocked);

    while (1) {
        ssize_t len = read(fan_fd, buf, sizeof(buf));
        if (len == -1) {
            if (errno == EAGAIN) continue;
            perror("read");
            exit(EXIT_FAILURE);
        }

        metadata = (struct fanotify_event_metadata*) buf;
        while (FAN_EVENT_OK(metadata, len)) {
            pid_t pid = metadata->pid;
            char proc_comm[256] = {0};
            char proc_path[PATH_MAX];
            char file_path[PATH_MAX] = {0};
            int is_blocked = 0;
            struct fanotify_response response = {
                .fd = metadata->fd,
                .response = FAN_ALLOW
            };

            char fd_path[64];
            snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", metadata->fd);
            ssize_t path_len = readlink(fd_path, file_path, sizeof(file_path) - 1);
            if (path_len > 0) {
                file_path[path_len] = '\0';
            } else {
                strcpy(file_path, "unknown");
                perror("readlink");
            }

            snprintf(proc_path, sizeof(proc_path), "/proc/%d/comm", pid);
            FILE *comm_file = fopen(proc_path, "r");
            
            if (comm_file) {
                if (fgets(proc_comm, sizeof(proc_comm), comm_file)) {
                    size_t ln = strlen(proc_comm) - 1;
                    if (proc_comm[ln] == '\n') proc_comm[ln] = '\0';
                    
                    if (strcmp(proc_comm, "task") == 0) {
                        printf("Process 'task' (PID:%d) opened file %s\n", pid, file_path);
                        
                        for (int i = 0; i < num_blocked; i++) {
                            if (strcmp(file_path, blocked_files[i]) == 0) {
                                printf("BLOCKED: access to protected file!\n");
                                response.response = FAN_DENY;
                                is_blocked = 1;
                                break;
                            }
                        }
                    }
                }
                fclose(comm_file);
            } else {
                perror("fopen(comm)");
            }

            if (write(fan_fd, &response, sizeof(response)) != sizeof(response)) {
                perror("write(fanotify response)");
            }
            close(metadata->fd);
            metadata = FAN_EVENT_NEXT(metadata, len);
        }
    }

    close(fan_fd);
    return 0;
}