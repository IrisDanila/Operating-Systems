#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_PATH_LENGTH 1024
#define MAX_METADATA_LENGTH 1024

// Function to generate metadata for a file or directory
void generate_metadata(const char *path, char *metadata) {
    // Get file status
    struct stat* file_stat = malloc(sizeof(struct stat));
    
    /*
    if (lstat(path, &file_stat) == -1) {
        perror("Failed to get file status");
        exit(EXIT_FAILURE);
    }
    */
    
    sprintf(metadata, "%s: Size=%ld, UID=%d, GID=%d, Permissions=%o\n",
            path, file_stat->st_size, file_stat->st_uid, file_stat->st_gid, file_stat->st_mode & 0777);

    free(file_stat);
}

// Function to create or update the snapshot file
void create_or_update_snapshot(const char *dir_path) {
    DIR *dir;
    struct dirent *entry;
    char snapshot_path[MAX_PATH_LENGTH];
    char entry_metadata[MAX_METADATA_LENGTH];

    // Open directory
    if ((dir = opendir(dir_path)) == NULL) {
        perror("Failed to open directory");
        exit(EXIT_FAILURE);
    }

    // Create snapshot file path
    snprintf(snapshot_path, MAX_PATH_LENGTH, "%s/Snapshot.txt", dir_path);

    // Open or create snapshot file
    int snapshot_fd = open(snapshot_path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);


    if (snapshot_fd == -1) {
        perror("Failed to open or create snapshot file");
        closedir(dir);
        exit(EXIT_FAILURE);
    }

    // Iterate through directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char entry_path[MAX_PATH_LENGTH];
        snprintf(entry_path, MAX_PATH_LENGTH, "%s/%s", dir_path, entry->d_name);

        // Generate metadata for the entry
        generate_metadata(entry_path, entry_metadata);

        // Write metadata to snapshot file
        if (write(snapshot_fd, entry_metadata, strlen(entry_metadata)) == -1) {
            perror("Failed to write metadata to snapshot file");
            closedir(dir);
            close(snapshot_fd);
            exit(EXIT_FAILURE);
        }
    }

    // Close directory and snapshot file
    closedir(dir);
    close(snapshot_fd);

    printf("Snapshot created or updated successfully.\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s directory_path\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *dir_path = argv[1];
    create_or_update_snapshot(dir_path);

    return 0;
}
