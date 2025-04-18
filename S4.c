// S4.c - Server Side for the .zip files //
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/stat.h>


#define PORT 1206
#define BUFFER_SIZE 4096

#include <errno.h>
#include <dirent.h>

// Creates a directory and all necessary parent directories in the given path.
 void make_directory(const char *path) {
    // Create a temporary copy of the path that we can modify
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);  // Copy original path to temporary buffer
    
    // Iterate through the path starting from the second character
    // (skip the first character to handle both absolute and relative paths)
    for (char *p = tmp + 1; *p; p++) {
        // When we encounter a directory separator
        if (*p == '/') {
            *p = 0;  // Temporarily truncate the string at this position
            
            // Create directory up to the current path segment
            // 0755 permissions: rwx for owner, rx for group/others
            mkdir(tmp, 0755);  
            
            *p = '/';  // Restore the separator for next iteration
        }
    }
    
    // Create the final directory in the complete path
    // This handles both cases:
    // 1. Paths without trailing slashes
    // 2. The final directory after processing all intermediate ones
    mkdir(tmp, 0755);
}

int main() {
    // Socket and network variables
    int server_fd, client_sock;  // File descriptors for server and client sockets
    struct sockaddr_in server, client;  // Server and client address structures
    socklen_t client_len = sizeof(client);  // Size of client address structure
    char buffer[BUFFER_SIZE];  // Buffer for network communication

    // Get home directory path from environment
    char *home = getenv("HOME");
    if (!home) {
        perror("Cannot get HOME environment");
        return 1;
    }

    // Create S4 directory if it doesn't exist (~/S4)
    char s4_folder[512];
    snprintf(s4_folder, sizeof(s4_folder), "%s/S4", home);
    mkdir(s4_folder, 0755);  // Create with rwxr-xr-x permissions

    // Create server socket (IPv4, TCP)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Configure server address structure
    server.sin_family = AF_INET;          // IPv4 address family
    server.sin_port = htons(PORT);        // Port number in network byte order
    server.sin_addr.s_addr = INADDR_ANY;  // Accept connections on all interfaces

    // Bind socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    // Start listening for incoming connections with a backlog of 3
    listen(server_fd, 3);
    printf("S4 server running on port %d...\n", PORT);

    // Main server loop - accept and handle client connections
    while ((client_sock = accept(server_fd, (struct sockaddr *)&client, &client_len))) {
        // Clear buffer before receiving new data
        memset(buffer, 0, sizeof(buffer));
        
        // Receive command from client
        int bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            close(client_sock);
            continue;
        }

        // Parse command and arguments (command, arg1, arg2)
        char command[20], arg1[256], arg2[256];
        sscanf(buffer, "%s %s %s", command, arg1, arg2);

        /* ========== Handle uploadf command (ZIP file upload) ========== */
        if (strcmp(command, "uploadf") == 0) {
            // Construct full destination path
            char real_dest_path[512];
            if (strncmp(arg2, "~S4", 3) == 0) {
                snprintf(real_dest_path, sizeof(real_dest_path), "%s/S4%s", home, arg2 + 3);
            } else {
                snprintf(real_dest_path, sizeof(real_dest_path), "%s/S4%s", home, arg2);
            }

            // Create any needed directories in the path
            make_directory(real_dest_path);

            // Create full file path
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", real_dest_path, arg1);

            // Open file for writing in binary mode
            FILE *f = fopen(filepath, "wb");
            if (!f) {
                perror("Error creating file");
                close(client_sock);
                continue;
            }

            // Receive and save file content
            while (1) {
                int bytes = recv(client_sock, buffer, BUFFER_SIZE, 0);
                if (bytes <= 0) break;  // Connection closed or error
                fwrite(buffer, 1, bytes, f);
                if (bytes < BUFFER_SIZE) break;  // Last chunk received
            }
            fclose(f);
            printf("[S4] Saved %s to %s\n", arg1, filepath);
        } 
        /* ========== Handle downlf command (ZIP file download) ========== */
        else if (strcmp(command, "downlf") == 0) {
            char *filename = arg1;
            char filepath[512];

            // Construct full file path
            if (strncmp(filename, "~S4", 3) == 0) {
                snprintf(filepath, sizeof(filepath), "%s/S4%s", home, filename + 3);
            } else {
                snprintf(filepath, sizeof(filepath), "%s/S4/%s", home, filename);
            }

            // Verify file is a ZIP file (S4 only handles ZIP files)
            char *ext = strrchr(filepath, '.');
            if (!ext || strcmp(ext, ".zip") != 0) {
                send(client_sock, "Error: Not a ZIP file.\n", 24, 0);
                close(client_sock);
                continue;
            }

            // Open file for reading
            FILE *f = fopen(filepath, "rb");
            if (!f) {
                send(client_sock, "Error: File not find on the Server.\n", 38, 0);
                perror("Error opening file for download");
                close(client_sock);
                continue;
            }

            // Send file content to client
            int bytes;
            while ((bytes = fread(buffer, 1, BUFFER_SIZE, f)) > 0) {
                send(client_sock, buffer, bytes, 0);
            }
            fclose(f);
            printf("[S4] Sent ZIP file %s\n", filepath);
        }
        /* ========== Handle removef command (ZIP file deletion) ========== */
        else if (strcmp(command, "removef") == 0) {
            char *filename = arg1;
            char filepath[512];

            // Construct full file path
            if (strncmp(filename, "~S4", 3) == 0) {
                snprintf(filepath, sizeof(filepath), "%s/S4%s", home, filename + 3);
            } else {
                snprintf(filepath, sizeof(filepath), "%s/S4/%s", home, filename);
            }

            // Verify file is a ZIP file
            char *ext = strrchr(filepath, '.');
            if (!ext || strcmp(ext, ".zip") != 0) {
                send(client_sock, "Error: Not a ZIP file.\n", 24, 0);
                close(client_sock);
                continue;
            }

            // Attempt to remove file
            if (remove(filepath) == 0) {
                send(client_sock, "ZIP file removed successfully.\n", 31, 0);
                printf("[S4] Removed ZIP file: %s\n", filepath);
            } else {
                send(client_sock, "Error: Could not remove ZIP file.\n", 35, 0);
                perror("Error removing file");
            }
        }
        /* ========== Handle dispfnames command (list ZIP files) ========== */
        else if (strcmp(command, "dispfnames") == 0) {
            char *home = getenv("HOME");
            if (!home) {
                close(client_sock);
                continue;
            }
        
            // Construct full directory path
            char full_path[512];
            if (strncmp(arg1, "~S4", 3) == 0) {
                snprintf(full_path, sizeof(full_path), "%s/S4%s", home, arg1 + 3);
            } else {
                snprintf(full_path, sizeof(full_path), "%s/%s", home, arg1);
            }
        
            // Open directory
            DIR *dir = opendir(full_path);
            if (!dir) {
                close(client_sock);
                continue;
            }
        
            // List all ZIP files in directory
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_REG) {  // Only regular files
                    char *ext = strrchr(entry->d_name, '.');
                    if (ext && (strcmp(ext, ".zip") == 0)) {  // Only ZIP files
                        send(client_sock, entry->d_name, strlen(entry->d_name), 0);
                        send(client_sock, "\n", 1, 0);
                    }
                }
            }
            closedir(dir);
        }
        
        // Close client connection
        close(client_sock);
    }

    // Close server socket (unreachable in normal operation)
    close(server_fd);
    return 0;
}