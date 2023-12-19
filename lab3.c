#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#define BUFFER_SIZE 8192

typedef struct {
    char source[PATH_MAX];
    char dest[PATH_MAX];
} path_data;

void *copy_file(void *arg) {
    path_data *paths = (path_data *)arg;
    int src_fd, dest_fd;
    ssize_t bytes_read, bytes_written;
    char buffer[BUFFER_SIZE];

    src_fd = open(paths->source, O_RDONLY);
    if (src_fd == -1) {
        perror("open source");
        goto cleanup;
    }

    dest_fd = open(paths->dest, O_WRONLY | O_CREAT, 0666);
    if (dest_fd == -1) {
        perror("open destination");
        close(src_fd);
        goto cleanup;
    }

    printf("Copying file from %s to %s\n", paths->source, paths->dest);

    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written == -1) {
            perror("write");
            break;
        }
    }

    if (bytes_read == -1) {
        perror("read");
    }

    close(src_fd);
    close(dest_fd);

cleanup:
    free(paths);
    return NULL;
}

void *copy_directory(void *arg) {
    path_data *paths = (path_data *)arg;
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char src_path[PATH_MAX];
    char dest_path[PATH_MAX];

    printf("Copying directory from %s to %s\n", paths->source, paths->dest);

    if ((dir = opendir(paths->source)) == NULL) {
        perror("opendir");
        return NULL;
    }

    mkdir(paths->dest, 0755);

    pthread_t thread_ids[100];  
    int thread_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        printf("Found in %s item: %s\n", paths->source, entry->d_name);

        snprintf(src_path, sizeof(src_path), "%s/%s", paths->source, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", paths->dest, entry->d_name);

        if (stat(src_path, &statbuf) == -1) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            path_data *new_paths = malloc(sizeof(path_data));
            strcpy(new_paths->source, src_path);
            strcpy(new_paths->dest, dest_path);

            printf("create copying directory from %s to %s\n", new_paths->source, new_paths->dest);

            if (thread_count < 100) {
                if (pthread_create(&thread_ids[thread_count], NULL, copy_directory, new_paths) != 0) {
                    perror("pthread_create");
                    free(new_paths);
                } else {
                    thread_count++;
                }
            }
        } else if (S_ISREG(statbuf.st_mode)) {
            //if (!is_file_copied(src_path)) {
                path_data *file_paths = malloc(sizeof(path_data));
                strcpy(file_paths->source, src_path);
                strcpy(file_paths->dest, dest_path);

                printf("create copying file from %s to %s\n", file_paths->source, file_paths->dest);

                if (thread_count < 100) {
                    if (pthread_create(&thread_ids[thread_count], NULL, copy_file, file_paths) != 0) {
                        perror("pthread_create");
                        free(file_paths);
                    } else {
                        thread_count++;
                        //mark_file_as_copied(src_path);
                    }
                }
            //}
        }
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(thread_ids[i], NULL);
    }

    closedir(dir);
    free(paths);

    return NULL;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("error, you need 'source directory' then 'destination directory'\n");
        return 1;
    }

    char *source = argv[1];
    char *destination = argv[2];

    path_data *paths = malloc(sizeof(path_data));
    if (paths == NULL) {
        perror("malloc");
        return 1;
    }

    strncpy(paths->source, source, PATH_MAX);
    paths->source[PATH_MAX - 1] = '\0';
    strncpy(paths->dest, destination, PATH_MAX);
    paths->dest[PATH_MAX - 1] = '\0';

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, copy_directory, paths) != 0) {
        perror("pthread_create");
        free(paths);
        return 1;
    }

    pthread_join(thread_id, NULL);

    return 0;
}
