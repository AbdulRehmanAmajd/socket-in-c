#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>

#define PORT 14000
#define STORAGE_LIMIT 10240  // 10 KB storage limit per client

struct client_info {
    int used_space;
} client;

void handle_upload(int new_socket, char *command) {
    char *file_path = strtok(command, "$");
    file_path = strtok(NULL, "$");

    if (client.used_space >= STORAGE_LIMIT) {
        send(new_socket, "$FAILURE$LOW_SPACE$", strlen("$FAILURE$LOW_SPACE$"), 0);
        return;
    }
    
    send(new_socket, "$SUCCESS$", strlen("$SUCCESS$"), 0);

    char *base_name = strrchr(file_path, '/');
    if (!base_name) {
        base_name = file_path;  // No '/' found, use the full path as the name
    } else {
        base_name++;  // Skip the '/'
    }

    const char *dir = "./server_dir";
    mkdir(dir, 0777);
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, base_name);

    FILE *file = fopen(full_path, "wb");
    if (!file) {
        perror("File open failed");
        return;
    }

    char buffer[1024];
    int bytes_read;
    while ((bytes_read = read(new_socket, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytes_read, file);
        client.used_space += bytes_read;

        if (client.used_space >= STORAGE_LIMIT) {
            send(new_socket, "$FAILURE$LOW_SPACE$", strlen("$FAILURE$LOW_SPACE$"), 0);
            break;
        }
    }

    fclose(file);
    send(new_socket, "$SUCCESS$", strlen("$SUCCESS$"), 0);
}

void handle_download(int new_socket, char *command) {
    char *file_name = strtok(command, "$");
    file_name = strtok(NULL, "$");

    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "./server_dir/%s", file_name);

    // Try to open the file in read mode
    FILE *file = fopen(full_path, "rb");
    if (!file) {
        send(new_socket, "$FAILURE$FILE_NOT_FOUND$", strlen("$FAILURE$FILE_NOT_FOUND$"), 0);
        return;
    }

    // Send $SUCCESS$ to notify client that file transfer will begin
    send(new_socket, "$SUCCESS$", strlen("$SUCCESS$"), 0);

    // Introduce a small delay to allow the client to process the success message
    usleep(100000);  // 100ms delay to ensure the client reads $SUCCESS$ before file data

    char buffer[1024];
    int bytes_read;

    // Read the file and send it to the client in chunks
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(new_socket, buffer, bytes_read, 0);  // Send file content
    }

    fclose(file);
    shutdown(new_socket, SHUT_WR);  // Signal EOF and close the writing end of the socket
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    client.used_space = 0;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Server is listening on 127.0.0.1:%d\n", PORT);
    
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        char command[1024] = {0};
        read(new_socket, command, 1024);

        if (strncmp(command, "$UPLOAD$", 8) == 0) {
            handle_upload(new_socket, command);
        } else if (strncmp(command, "$DOWNLOAD$", 10) == 0) {
            handle_download(new_socket, command);
        }

        close(new_socket);
    }

    close(server_fd);
    return 0;
}
