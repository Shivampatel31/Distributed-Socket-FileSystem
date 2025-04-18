// w25clients.c
// Client-side implementation for COMP-8567 Distributed File System Project
// Handles commands: uploadf, downlf, removef, downltar, dispfnames
// w25clients.c - Client-side code (Final Version)
#include <stdio.h>      // For input/output functions like printf(), scanf(), fopen(), etc.
#include <stdlib.h>     // For memory allocation (malloc, free), exit(), and general utilities
#include <string.h>     // For string operations like strcpy(), strcat(), strlen(), strcmp(), memset(), etc.
#include <unistd.h>     // For POSIX functions like read(), write(), close(), fork(), sleep(), etc.
#include <sys/socket.h> // For socket programming: socket(), bind(), connect(), send(), recv(), etc.
#include <arpa/inet.h>  // For functions related to IP address conversion like inet_pton(), htons(), etc.
#include <netinet/in.h> // For sockaddr_in structure used in networking
#include <pthread.h>    // For working with threads: pthread_create(), pthread_exit(), etc.
#include <errno.h>      // For error codes like EEXIST, EINVAL, etc.
#include <sys/stat.h>   // For mkdir(), stat(), and other file/directory-related operations



#define PORT 1221

#include <dirent.h> // This header is for reading directories — listing files and folders inside a directory.

#define SERVER_IP "127.0.0.1"  // Server IP Address 
#define BUFFER_SIZE 4096
/* Send a command string to the server through the socket */
void send_command(int sock, char *command) {
    // Send the complete command string to server
    // sock: The connected socket descriptor
    // command: The command string to send
    // strlen(command): Length of the command string
    // 0: Default flags for send() 
    send(sock, command, strlen(command), 0);
}

/* Receive and print response from server until complete */
/*
  The recv() Function: receiving data from a socket
    ssize_t recv(int sockfd, void *buf, size_t len, int flags);
    sockfd: The socket descriptor
    buf: Buffer to store received data
    len: Maximum length of data to receive (BUFFER_SIZE in your case)
    flags: Optional flags (0 means no special behavior)
*/
void receive_response(int sock) {
    char buffer[BUFFER_SIZE];  // Buffer to store received data
    int bytes;  // Number of bytes received
    int expecting_more = 1;
   
    
    // Continuously receive data until server closes connection
    while ((bytes = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes] = '\0';  // Null-terminate the received data
        printf("%s", buffer);  // Print the received data
        
        // If received less than full buffer, we got all data
        if (bytes < BUFFER_SIZE) break;
    }
    if (strstr(buffer, "ENDOFLIST")) {
        expecting_more = 0;  // No more data after this
        char *end = strstr(buffer, "ENDOFLIST");
        *end = '\0';  // Terminate string before marker
    }
}

/* Upload a file to the server */
void upload_file(int sock, char *filename, char *destination_path) {
    // Open the file in binary read mode
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("File not found");  // Print error if file doesn't exist
        return;
    }

    /*
      snprintf() is a secure string formatting function that:
        Formats data into a string (like printf)
        Writes the result to a buffer
        Guarantees it won't overflow the buffer by limiting the number of characters written 
    */
    // Format the upload command with filename and destination path
    char command[512];
    snprintf(command, sizeof(command), "uploadf %s %s", filename, destination_path);
    
    // Send the upload command to server
    send_command(sock, command);
    sleep(1);  // Brief delay to ensure server is ready for file data

    // Read file contents and send to server in chunks
    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(sock, buffer, bytes, 0);  // Send each chunk
    }
    fclose(file);  // Close the file when done
   
    // Wait for and print server's response to upload
    receive_response(sock);
}

/* Download a file from the server */
void download_file(int sock, char *filepath) {
    // Format the download command with full file path
    char command[512];
    snprintf(command, sizeof(command), "downlf %s", filepath);
    
    // Send download command to server
    send_command(sock, command);
    sleep(1);  // Brief delay to allow server to prepare file

    // Extract just the filename from the full path
    char *filename = strrchr(filepath, '/');
    if (filename == NULL) {
        filename = filepath;  // If no path, use whole string
    } else {
        filename++;  // Skip the '/' character
    }

    // Create a new file to store downloaded data
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Error creating file");
        return;
    }

    // Receive file data from server
    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        // Check if server sent an error message instead of file data
        if (bytes < BUFFER_SIZE && buffer[0] == 'E') {
            printf("%s", buffer);  // Print error message
            fclose(file);
            remove(filename);  // Delete any partially downloaded file
            return;
        }
        
        // Write received data to file
        fwrite(buffer, 1, bytes, file);
        
        // If received less than full buffer, transfer is complete
        if (bytes < BUFFER_SIZE) break;
    }
    fclose(file);
    printf("File downloaded: %s\n", filename);  // Confirm successful download
}


/* Request server to remove a file */
void remove_file(int sock, char *filepath) {
    // Format the remove command with file path
    char command[512];
    snprintf(command, sizeof(command), "removef %s", filepath);
    
    // Send remove command to server
    send_command(sock, command);
    
    // Wait for and display server's response
    char response[BUFFER_SIZE];
    int bytes = recv(sock, response, BUFFER_SIZE, 0);
    if (bytes > 0) {
        response[bytes] = '\0';  // Null-terminate the response
        printf("%s", response);  // Print server's response
    }
}

/* Download a tar archive of specific file type from server */
void download_tar(int sock, char *filetype) {
    // Validate requested file type against supported types
    if (strcmp(filetype, ".c") != 0 && 
        strcmp(filetype, ".pdf") != 0 && 
        strcmp(filetype, ".txt") != 0) {
        printf("Error: Only .c, .pdf, or .txt file types are supported.\n");
        return;
    }

    // Determine output tar filename based on file type
    char tarname[20];  // Buffer for tar filename
    if (strcmp(filetype, ".c") == 0)
        strcpy(tarname, "cfiles.tar");    // C files archive
    else if (strcmp(filetype, ".pdf") == 0)
        strcpy(tarname, "pdf.tar");       // PDF files archive
    else if (strcmp(filetype, ".txt") == 0)
        strcpy(tarname, "text.tar");      // Text files archive

    // Format and send download command to server
    char command[512];
    snprintf(command, sizeof(command), "downltar %s", filetype);
    send_command(sock, command);
    sleep(1);  // Allow server time to prepare the tar file

    // Check for error response from server (peek without removing from queue)
    char response[BUFFER_SIZE];
    int bytes = recv(sock, response, BUFFER_SIZE, MSG_PEEK | MSG_DONTWAIT);
    if (bytes > 0 && response[0] == 'E') {
        // Actually read the error message to clear buffer
        bytes = recv(sock, response, BUFFER_SIZE, 0);
        response[bytes] = '\0';
        printf("%s", response);  // Display server error
        return;
    }

    // Create output file for the tar archive
    FILE *file = fopen(tarname, "wb");  // Open in binary write mode
    if (!file) {
        printf("Error: Could not create output file.\n");
        return;
    }

    // Receive and save the tar file contents
    char buffer[BUFFER_SIZE];
    int total_bytes = 0;
    while ((bytes = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        // Check if server sent an error message instead of file data
        if (bytes < BUFFER_SIZE && buffer[0] == 'E') {
            printf("%.*s", bytes, buffer);  // Print error message
            fclose(file);
            remove(tarname);  // Delete incomplete/empty tar file
            return;
        }
        
        // Write received data to file
        fwrite(buffer, 1, bytes, file);
        total_bytes += bytes;  // Track total bytes received
        
        // If received less than full buffer, transfer is complete
        if (bytes < BUFFER_SIZE) break;
    }
    fclose(file);  // Close the tar file

    // Verify we actually received data
    if (total_bytes == 0) {
        printf("Error: No files of type %s found on server.\n", filetype);
        remove(tarname);  // Remove empty tar file
        return;
    }

    // Success message with downloaded archive info
    printf("Successfully downloaded %s containing all %s files.\n", tarname, filetype);
}

/* Display list of files in specified directory from server */
void display_filenames(int sock, char *pathname) {
    // Send directory listing request to server
    char command[512];
    snprintf(command, sizeof(command), "dispfnames %s", pathname);
    send(sock, command, strlen(command), 0);

    // Print directory header
    printf("Files in %s:\n", pathname);

    // Receive and process server response
    char buffer[BUFFER_SIZE];
    int bytes;
    int expecting_more = 1;  // Flag to track if more data is coming
    
    while (expecting_more) {
        bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) break;  // Connection closed or error
        
        buffer[bytes] = '\0';  // Null-terminate received data
        
        // Check for end-of-transmission marker
        if (strstr(buffer, "ENDOFLIST")) {
            expecting_more = 0;  // No more data after this
            char *end = strstr(buffer, "ENDOFLIST");
            *end = '\0';  // Terminate string before marker
        }
        
        printf("%s", buffer);  // Print received filenames
        fflush(stdout);       // Ensure immediate display
    }
    
    // printf("\n");  // Final newline for clean output
}

/* Main client program entry point */
int main() {
    int sock;  // Socket file descriptor for server connection
    struct sockaddr_in server_addr;  // Server address structure

    /* Create a TCP socket for communication */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");  // Print system error if socket creation fails
        return 1;  // Exit with error code
    }

    /* Configure server address structure */
    server_addr.sin_family = AF_INET;  // Use IPv4 address family
    server_addr.sin_port = htons(PORT);  // Convert port number to network byte order
    /* Convert IP address from text to binary form and store in address structure */
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    /* Establish connection to the server */
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");  // Print system error if connection fails
        return 1;  // Exit with error code
    }

    /* Connection established - print welcome message */
    printf("Connected to S1 server. Enter commands below:\n");

    /* Command processing variables */
    char input[512];   // Buffer for user input
    char command[20];  // Extracted command
    char arg1[256];    // First argument (if any)
    char arg2[256];    // Second argument (if any)

    /* Main command loop - runs until user exits */
    while (1) {
        /* Display command prompt */
        printf("w25clients$ ");
        fflush(stdout);  // Ensure prompt is displayed immediately

        /* Get user input */
        // fgets - It is commonly used for reading input from a file or from standard input (stdin).
        fgets(input, sizeof(input), stdin);
        /* Remove trailing newline character from input */
        // The strcspn() function searches for the first occurrence in a string of any of the specified characters
        input[strcspn(input, "\n")] = 0;

        /* Parse input into command and arguments */
        // sscanf - function reads formatted input from a string and stores the result in the provided variables.
        if (sscanf(input, "%s %s %s", command, arg1, arg2) >= 1) {
            /* Execute appropriate command based on user input */
            if (strcmp(command, "uploadf") == 0)
                upload_file(sock, arg1, arg2);       // Handle file upload
            else if (strcmp(command, "downlf") == 0)
                download_file(sock, arg1);           // Handle file download
            else if (strcmp(command, "removef") == 0)
                remove_file(sock, arg1);             // Handle file removal
            else if (strcmp(command, "downltar") == 0)
                download_tar(sock, arg1);            // Handle tar file download
            else if (strcmp(command, "dispfnames") == 0)
                display_filenames(sock, arg1);       // Handle directory listing
            else if (strcmp(command, "exit") == 0)
                return 0;
            else
                printf("Invalid command.\n");        // Unknown command handler
        }
    }

    /* Cleanup (though this may never be reached due to infinite loop) */
    close(sock);  // Close network socket
    return 0;     // Exit successfully
}