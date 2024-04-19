#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <wait.h>

#define MAX_PATH_LENGTH 1024
#define MAX_METADATA_LENGTH 1024

void generate_metadata(const char *path, char *metadata) {

    struct stat *file_stat = malloc(sizeof(struct stat));

    if (lstat(path, file_stat) == -1) {
        perror("Failed to get file status");
        exit(EXIT_FAILURE);
    }
    
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    sprintf(metadata, "%s:\n Marca temporala=%d-%02d-%02d %02d:%02d:%02d\n Size=%ld bytes\n Permissions=%d\n Ultima modificare=%s Numar Inode=%ld\n\n",
            path,  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec ,file_stat->st_size, 
            file_stat->st_mode & 0777, ctime(&file_stat->st_ctime) ,file_stat->st_ino);

    free(file_stat); 
}

void create_or_update_snapshot(const char *dir_path, const char *output_dir) {
    DIR *dir;
    struct dirent *entry;
    char snapshot_path[MAX_PATH_LENGTH];
    char entry_metadata[MAX_METADATA_LENGTH];

    // Open directory
    if ((dir = opendir(dir_path)) == NULL) {
        perror("Failed to open directory");
        exit(EXIT_FAILURE);
    }

    // Create snapshot file path in the output directory
    snprintf(snapshot_path, MAX_PATH_LENGTH, "%s/Snapshot.txt", output_dir);

    // Open or create snapshot file
    int snapshot_fd = open(snapshot_path, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR);
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

        // Write metadata to snapshot file only if it's not a directory
        if(entry->d_type!=4){
            if (write(snapshot_fd, entry_metadata, strlen(entry_metadata)) == -1) {
                perror("Failed to write metadata to snapshot file");
                closedir(dir);
                close(snapshot_fd);
                exit(EXIT_FAILURE);
            }
        }
        
        // If entry is a directory, recursively call create_or_update_snapshot()
        if (entry->d_type == 4) {  /// DT_DIR not working 
            create_or_update_snapshot(entry_path, output_dir);
        }
    }

    // Close directory and snapshot file
    closedir(dir);
    close(snapshot_fd);

    printf("Snapshot for directory %s created or updated successfully.\n", dir_path);
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 12) {
        exit(EXIT_FAILURE);
    }

    const char *output_dir = argv[2];

    for (int i = 3; i < argc; i++) {
        
        pid_t pid=fork();
        if(pid==-1)
        {
            perror("Fork failed\n");
            exit(-1);
        }
        else if(pid==0)
        {
            create_or_update_snapshot(argv[i], output_dir);
            /// exit(EXIT_SUCCESS);
        }

        int status;
        while(wait(&status)>0);
    }

    return 0;
}
