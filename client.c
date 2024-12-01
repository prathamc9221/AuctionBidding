#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define LOG_BUFFER_SIZE 2048

FILE *log_file;

void log_message(const char *message) {
    time_t now;
    time(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(log_file, "[%s] %s\n", timestamp, message);
    fflush(log_file);
}

void display_help() {
    printf("\nBidding Instructions:\n");
    printf("1. Enter the auction ID and bid amount in format: <auction_id> <amount>\n");
    printf("2. Enter 'ls' to view the current bids of all auctions\n");
    printf("3. Minimum bid must be at least 20%% higher than the current bid\n");
    printf("4. Enter 'q' to quit\n\n");
}

int main() {
    log_file = fopen("client_log.txt", "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
    
    log_message("Client starting up...");

    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char log_buffer[LOG_BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_message("Socket creation failed");
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_message("Connection to server failed");
        perror("Connection to server failed");
        close(sock);
        return 1;
    }

    log_message("Connected to the auction server");
    printf("Connected to the auction server.\n");
    display_help();

    int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
        
        snprintf(log_buffer, LOG_BUFFER_SIZE, "Received initial auction list: %s", buffer);
        log_message(log_buffer);
    } else {
        log_message("Error receiving initial auction list from server");
        perror("Error receiving initial auction list from server");
    }

    while (1) {
        printf("\nEnter auction ID and bid amount (or 'ls' to list current auctions, 'q' to quit): ");
        fgets(buffer, BUFFER_SIZE, stdin);

        // Handle "quit" command
        if (buffer[0] == 'q' || buffer[0] == 'Q') {
            log_message("User initiated quit command");
            break;
        }

        // Handle "ls" command
        if (strncmp(buffer, "ls", 2) == 0 || strncmp(buffer, "LS", 2) == 0) {
            log_message("User requested current auction list");

            if (send(sock, "ls", 2, 0) < 0) {
                log_message("Error sending 'ls' request");
                perror("Error sending 'ls' request");
                break;
            }

            bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                printf("%s", buffer);
                snprintf(log_buffer, LOG_BUFFER_SIZE, "Received current auction list: %s", buffer);
                log_message(log_buffer);
            } else {
                log_message("Error receiving auction list from server");
                perror("Error receiving auction list from server");
            }

            continue;
        }

        snprintf(log_buffer, LOG_BUFFER_SIZE, "Sending bid: %s", buffer);
        log_message(log_buffer);

        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            log_message("Error sending bid");
            perror("Error sending bid");
            break;
        }

        bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("%s", buffer);
            
            snprintf(log_buffer, LOG_BUFFER_SIZE, "Received server response: %s", buffer);
            log_message(log_buffer);
        } else if (bytes_received == 0) {
            log_message("Server closed the connection");
            printf("Server closed the connection.\n");
            break;
        } else {
            log_message("Error receiving update from server");
            perror("Error receiving update from server");
        }
    }

    log_message("Client shutting down");
    printf("Thank you for participating in the auction!\n");
    fclose(log_file);
    close(sock);
    return 0;
}
