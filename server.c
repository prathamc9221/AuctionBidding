//server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define LOG_BUFFER_SIZE 2048
#define MIN_BID_INCREASE 1.20  // 20% increase requirement

FILE *log_file;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int auction_id;
    char item_name[50];
    float current_bid;
    int highest_bidder;
} Auction;

Auction auctions[5];
int num_auctions = 5;
int num_clients = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Hardcoded user credentials
typedef struct {
    char username[50];
    char password[50];
} User;

User users[] = {
    {"user1", "pass1"},
    {"user2", "pass2"},
    {"admin", "admin123"}
};
int num_users = sizeof(users) / sizeof(users[0]);

void log_message(const char *message) {
    time_t now;
    time(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    pthread_mutex_lock(&log_lock);
    fprintf(log_file, "[%s] %s\n", timestamp, message);
    fflush(log_file);
    pthread_mutex_unlock(&log_lock);
}

void initialize_auctions() {
    auctions[0].auction_id = 1;
    strcpy(auctions[0].item_name, "Antique Vase");
    auctions[0].current_bid = 50.0;
    auctions[0].highest_bidder = -1;

    auctions[1].auction_id = 2;
    strcpy(auctions[1].item_name, "Vintage Car");
    auctions[1].current_bid = 500.0;
    auctions[1].highest_bidder = -1;

    auctions[2].auction_id = 3;
    strcpy(auctions[2].item_name, "Adventure 360");
    auctions[2].current_bid = 200.0;
    auctions[2].highest_bidder = -1;

    auctions[3].auction_id = 4;
    strcpy(auctions[3].item_name, "Yamaha 340");
    auctions[3].current_bid = 180.0;
    auctions[3].highest_bidder = -1;

    auctions[4].auction_id = 5;
    strcpy(auctions[4].item_name, "Tata Power");
    auctions[4].current_bid = 350.0;
    auctions[4].highest_bidder = -1;
    
    char log_buffer[LOG_BUFFER_SIZE];
    snprintf(log_buffer, LOG_BUFFER_SIZE, "Auctions initialized: %d items", num_auctions);
    log_message(log_buffer);
}

int authenticate_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    char username[50], password[50];
    
    // Request username
    send(client_socket, "Enter username: ", 17, 0);
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) return 0;
    buffer[bytes_received] = '\0';
    strcpy(username, buffer);

    // Request password
    send(client_socket, "Enter password: ", 17, 0);
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) return 0;
    buffer[bytes_received] = '\0';
    strcpy(password, buffer);

    // Check credentials
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0 && strcmp(users[i].password, password) == 0) {
            send(client_socket, "Authentication successful\n", 26, 0);
            return 1;
        }
    }

    send(client_socket, "Authentication failed. Disconnecting...\n", 40, 0);
    return 0;
}

void *client_handler(void *arg) {
    int client_socket = *((int *)arg);
    int client_id = num_clients++;
    char log_buffer[LOG_BUFFER_SIZE];
    
    if (!authenticate_client(client_socket)) {
        snprintf(log_buffer, LOG_BUFFER_SIZE, "Authentication failed for client");
        log_message(log_buffer);
        close(client_socket);
        return NULL;
    }

    snprintf(log_buffer, LOG_BUFFER_SIZE, "New client connected. Client ID: %d", client_id);
    log_message(log_buffer);

    char buffer[BUFFER_SIZE];
    sprintf(buffer, "Welcome to the Auction! Current items:\n");

    for (int i = 0; i < num_auctions; i++) {
        char auction_info[BUFFER_SIZE];
        snprintf(auction_info, BUFFER_SIZE,
                "Auction ID %d: %s, Current Bid: %.2f (Minimum next bid: %.2f)\n",
                auctions[i].auction_id, auctions[i].item_name, 
                auctions[i].current_bid, auctions[i].current_bid * MIN_BID_INCREASE);
        strcat(buffer, auction_info);
    }

    if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
        log_message("Error sending initial auction list to client");
        perror("Error sending initial auction list to client");
    }

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            snprintf(log_buffer, LOG_BUFFER_SIZE, "Client %d disconnected", client_id);
            log_message(log_buffer);
            break;
        }

        buffer[bytes_received] = '\0';

        if (strncmp(buffer, "ls", 2) == 0) {
            snprintf(log_buffer, LOG_BUFFER_SIZE, "Client %d requested current auction list", client_id);
            log_message(log_buffer);

            char auction_list[BUFFER_SIZE] = "Current Auction List:\n";
            for (int i = 0; i < num_auctions; i++) {
                char auction_info[BUFFER_SIZE];
                snprintf(auction_info, BUFFER_SIZE,
                        "Auction ID %d: %s, Current Bid: %.2f (Minimum next bid: %.2f)\n",
                        auctions[i].auction_id, auctions[i].item_name, 
                        auctions[i].current_bid, auctions[i].current_bid * MIN_BID_INCREASE);
                strcat(auction_list, auction_info);
            }

            if (send(client_socket, auction_list, strlen(auction_list), 0) < 0) {
                log_message("Error sending auction list to client");
                perror("Error sending auction list to client");
            }

            continue;
        }

        int auction_id;
        float bid_amount;
        sscanf(buffer, "%d %f", &auction_id, &bid_amount);

        snprintf(log_buffer, LOG_BUFFER_SIZE, "Received bid from Client %d: Auction ID %d, Amount %.2f", 
                client_id, auction_id, bid_amount);
        log_message(log_buffer);

        pthread_mutex_lock(&lock);
        int valid_bid = 0;

        for (int i = 0; i < num_auctions; i++) {
            if (auctions[i].auction_id == auction_id) {
                float minimum_bid = auctions[i].current_bid * MIN_BID_INCREASE;
                
                if (bid_amount >= minimum_bid) {
                    auctions[i].current_bid = bid_amount;
                    auctions[i].highest_bidder = client_id;

                    snprintf(buffer, BUFFER_SIZE,
                            "Update: Auction ID %d: New highest bid is %.2f by Client %d\nMinimum next bid required: %.2f\n",
                            auction_id, bid_amount, client_id, bid_amount * MIN_BID_INCREASE);
                    
                    snprintf(log_buffer, LOG_BUFFER_SIZE, "New highest bid: Auction ID %d, Amount %.2f, Client %d",
                            auction_id, bid_amount, client_id);
                    log_message(log_buffer);
                    
                    valid_bid = 1;
                    break;
                } else {
                    snprintf(buffer, BUFFER_SIZE,
                            "Error: Bid must be at least %.2f (20%% more than current bid of %.2f)\n",
                            minimum_bid, auctions[i].current_bid);
                    
                    snprintf(log_buffer, LOG_BUFFER_SIZE, 
                            "Invalid bid (below minimum increase): Auction ID %d, Amount %.2f, Client %d",
                            auction_id, bid_amount, client_id);
                    log_message(log_buffer);
                }
            }
        }

        if (!valid_bid) {
            snprintf(buffer, BUFFER_SIZE, "Error: Invalid auction ID or bid amount\n");
            log_message("Invalid bid received");
        }

        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            log_message("Error sending response to client");
            perror("Error sending response to client");
        }
        
        pthread_mutex_unlock(&lock);
    }

    close(client_socket);
    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    log_file = fopen("server_log.txt", "a");
    if (log_file == NULL) {
        perror("Unable to open log file");
        exit(EXIT_FAILURE);
    }

    initialize_auctions();

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d\n", PORT);
    log_message("Server started");

    while (1) {
        addr_size = sizeof(client_addr);
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size)) < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler, &client_socket) != 0) {
            perror("Thread creation failed");
            continue;
        }

        pthread_detach(thread_id);
    }

    fclose(log_file);
    return 0;
}
