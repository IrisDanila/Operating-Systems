#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <libgen.h>

#define MAX_PATH_LENGTH 1024
#define MAX_METADATA_LENGTH 1024
#define MALICIOUS_DIR_NAME "MaliciousFiles"
#define READ_END 0
#define WRITE_END 1

// Function to generate metadata for a given file
void generate_metadata(const char *path, char *metadata) {
    struct stat *file_stat = malloc(sizeof(struct stat));

    if (lstat(path, file_stat) == -1) {
        perror("Failed to get file status");
        exit(EXIT_FAILURE);
    }
    
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    sprintf(metadata, "%s:\n Marca temporala=%d-%02d-%02d %02d:%02d:%02d\n Dimensiune=%ld bytes\n Permisiuni=%d\n Ultima modificare=%s Numar Inode=%ld\n\n",
            path,  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec ,file_stat->st_size, 
            file_stat->st_mode & 0777, ctime(&file_stat->st_ctime) ,file_stat->st_ino);

    free(file_stat); 
}

bool current_file_is_malicious;

// Function to execute the malicious check script on a file
void execute_malicious_check_script(const char *file_path) {
    int pipe_fd[2]; // Declare the pipe file descriptors
    
    if (pipe(pipe_fd) == -1) { // Create the pipe
        perror("Pipe creation failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork(); // Create a new process

    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        close(pipe_fd[READ_END]); // Close unused read end
        
        close(pipe_fd[WRITE_END]); // Close write end
        // Execute the malicious check script
        execl("./verify_for_malicious.sh", "./verify_for_malicious.sh", file_path, NULL);
        // If execl returns, it means it failed
        perror("execl failed");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        close(pipe_fd[WRITE_END]); // Close unused write end
        // Read the result from the pipe
        char result[MAX_PATH_LENGTH];
        ssize_t bytes_read = read(pipe_fd[READ_END], result, MAX_PATH_LENGTH - 1); // Read into buffer
        if (bytes_read == -1) {
            perror("Failed to read from pipe");
            exit(EXIT_FAILURE);
        }
        result[bytes_read] = '\0'; // Null-terminate the string
        close(pipe_fd[READ_END]); // Close read end
        // Print the result
        printf("Result for file %s:\n%s\n", file_path, result);

        // Check the exit status of the script
        int status;
        waitpid(pid, &status, 0);
        printf("STATUS : %d\n\n", WEXITSTATUS(status));
        current_file_is_malicious = false;
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 2) {
                // Move the file to the MaliciousFiles directory
                char destination[MAX_PATH_LENGTH];
                snprintf(destination, MAX_PATH_LENGTH, "%s/%s", MALICIOUS_DIR_NAME, basename((char*)file_path));
                current_file_is_malicious = true;

                if (rename(file_path, destination) != 0) {
                    perror("Failed to move file to MaliciousFiles directory");
                    exit(EXIT_FAILURE);
                }
                printf("File %s moved to %s\n", file_path, MALICIOUS_DIR_NAME);
            } else {
                printf("File %s is safe\n", file_path);
            }
        } else {
            printf("Child process terminated abnormally\n");
        }
    }
}

// Function to create or update a snapshot of a directory
int create_or_update_snapshot(const char *dir_path, const char *output_dir) {
    DIR *dir;
    struct dirent *entry;
    char snapshot_path[MAX_PATH_LENGTH];
    char entry_metadata[MAX_METADATA_LENGTH];
    int corrupted_files_count = 0;

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

        if(entry->d_type == DT_REG){
            if ((access(entry_path, R_OK) == -1) || (access(entry_path, W_OK)==-1)) {
                execute_malicious_check_script(entry_path);
                if(current_file_is_malicious==true)
                    corrupted_files_count++;
            }
        }

        // Write metadata to snapshot file only if it's not a directory
        if(entry->d_type != DT_DIR) {
            if (write(snapshot_fd, entry_metadata, strlen(entry_metadata)) == -1) {
                perror("Failed to write metadata to snapshot file");
                closedir(dir);
                close(snapshot_fd);
                exit(EXIT_FAILURE);
            }
        }
        
        // If entry is a directory, recursively call create_or_update_snapshot()
        if (entry->d_type == DT_DIR) {
            corrupted_files_count += create_or_update_snapshot(entry_path, output_dir);
        }
    }

    // Close directory and snapshot file
    closedir(dir);
    close(snapshot_fd);

    printf("Snapshot for directory %s created or updated successfully. Corrupted files found: %d\n\n", dir_path, corrupted_files_count);

    return corrupted_files_count;
}

int main(int argc, char *argv[]) {
    if (argc < 5 || argc > 14) {
        exit(EXIT_FAILURE);
    }

    const char *output_dir = argv[2];
    int total_corrupted_files = 0;

    // Clear contents of Snapshot.txt file in the output directory
    char snapshot_path[MAX_PATH_LENGTH];
    snprintf(snapshot_path, MAX_PATH_LENGTH, "%s/Snapshot.txt", output_dir);
    int snapshot_fd = open(snapshot_path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    if (snapshot_fd == -1) {
        perror("Failed to clear Snapshot.txt file");
        exit(EXIT_FAILURE);
    }
    close(snapshot_fd);

    for (int i = 5; i < argc; i++) {
        
        pid_t pid=fork();
        if(pid==-1)
        {
            perror("Fork failed\n");
            exit(-1);
        }
        else if(pid==0)
        {
            int corrupted_files_count = create_or_update_snapshot(argv[i], output_dir);
            exit(corrupted_files_count);
        }
        else {
            int child_status;
            wait(&child_status);
            if (WIFEXITED(child_status)) {
                total_corrupted_files += WEXITSTATUS(child_status);
            }
        }
    }

    printf("Total corrupted files found: %d\n", total_corrupted_files);

    return 0;
}
