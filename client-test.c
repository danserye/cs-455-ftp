#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#define MAX_SIZE 1024
//For each function/method, explain its purpose, explain the values and
//meaning of parameters, return value, and any possible side effects;


char getLocalAddr() {
    struct addrinfo hints, *res;
    int status;
    char ipstr[INET6_ADDRSTRLEN];

    // initialize the hints struct
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever is available
    hints.ai_socktype = SOCK_STREAM; // use TCP

    // get the address info for the localhost
    //Instance ID: 4-8
    if ((status = getaddrinfo("localhost", NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // loop through all the results and get the first IP address
    void *addr;
    char *ipver;
    for(struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        // get the pointer to the address itself
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        // convert the IP address to a string and print it
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        printf("%s: %s\n", ipver, ipstr);
        break; // get the first IP address only
    }

    freeaddrinfo(res); // free the linked list

    return *ipstr;
}

//Instance ID: 1-1
//using recv with buffer size instead of without
void receive_cmd(int sock_fd, char *buffer, size_t buffer_size) {
    int bytes_received = 0;
    while (bytes_received <= 0) {
        bytes_received = recv(sock_fd, buffer, buffer_size, 0);
    }
    buffer[bytes_received] = '\0';
}

// Generate a random number in the range of the registered TCP ports for use on the client
//Instance ID: 7-2, avoiding ports below 1024 as they are restricted
int port_number() {
    int cntrl_port;
    srand(time(NULL));  // Seed the random number generator with the current time

    cntrl_port = rand() % (49151 - 1024 + 1) + 1024; // Generate a random number between 1024 and 49151 (registered ports)

    return cntrl_port;
}

// Check to see if the port generated above is in use; if it is, try again
//Instance ID: 7-1, avoiding using a port that is in use
int is_port_in_use(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //Instance ID: 4-1, catching exception
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int bind_res = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    close(sockfd);

    if (bind_res < 0) {
        if (errno == EADDRINUSE) {
            is_port_in_use(port_number());
        } else {
            is_port_in_use(port_number());
        }
    } else {
        if (is_port_in_use(port+1)) {
            is_port_in_use(port_number());
        }
        return 0;
    }
    // "Control may reach end of non-void function" warning -- should I care?
}

int main () {

    char server_cntrl_address[MAX_SIZE]; // For demo purposes, client and server are on the same machine and so use localhost addr
    char server_data_address [MAX_SIZE]; // For demo purposes, client and server are on the same machine and so use localhost addr
    int cntrl_port = port_number();
    char username[MAX_SIZE];
    char password[MAX_SIZE];
    char file[MAX_SIZE];
    char command[MAX_SIZE];
    char buffer[MAX_SIZE];
    char go_again[2];

    // Used for strncmp purposes
    char RETR[] = "RETR";
    char CWD[] = "CWD";
    char STOR[] = "STOR";
    char LS[] = "ls";
    char PWD[] = "pwd";
    char EXIT[] = "exit";

    // Prompt user for server IP address
    printf("Enter server address: ");
    scanf("%s", server_cntrl_address);

// CONTROL SOCKET #####################################################################################################


    // Create control socket
    //Instance ID: 4-2, catching exception
    int cntrl_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (cntrl_sock_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the control port
    struct sockaddr_in client_cntrl_addr = {0};
    client_cntrl_addr.sin_family = AF_INET;
    client_cntrl_addr.sin_addr.s_addr = htonl(getLocalAddr());
    client_cntrl_addr.sin_port = htons(cntrl_port);
    socklen_t server_cntrl_addr_len = sizeof(client_cntrl_addr);

    //Instance ID: 4-3, avoiding error by checking binding
    if (bind(cntrl_sock_fd, (struct sockaddr *)&client_cntrl_addr, sizeof(client_cntrl_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening on the socket
    //Instance ID: 4-4
    if (listen(cntrl_sock_fd, 1) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

// CONTROL CONNECTION TO SERVER

    // Form control connection to the server
    struct sockaddr_in server_cntrl_addr;
    memset(&server_cntrl_addr, 0, sizeof(server_cntrl_addr));
    server_cntrl_addr.sin_family = AF_INET;
    server_cntrl_addr.sin_port = htons(cntrl_port);
    is_port_in_use(cntrl_port);

    //Instance ID: 4-5
    if (inet_pton(AF_INET, server_cntrl_address, &server_cntrl_addr.sin_addr) <= 0) {
        perror("Invalid server address");
        exit(EXIT_FAILURE);
    }

    //Instance ID: 4-6
    if (connect(cntrl_sock_fd, (struct sockaddr *)&server_cntrl_addr, sizeof(server_cntrl_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Write the localhost address to the buffer and send to the server
    snprintf(buffer, sizeof(buffer), "%c %d", getLocalAddr(), cntrl_port);


    if (send(cntrl_sock_fd, buffer, strlen(buffer), 0) < 0) {
        printf("Error sending PORT\n");
        close(cntrl_sock_fd);
        exit(1);
    }

    while (1) {

        printf("What would you like to do? ");
        scanf("%s", command);

        // COMMAND HANDLERS

        // EXIT
        if (strncmp(command, EXIT, 4) == 0) {
            sprintf(buffer, "EXIT %s\r\n", file);
            send(cntrl_sock_fd, buffer, strlen(buffer), 0);
            close(cntrl_sock_fd);
            printf("Connections closed! Have a bad day.");
            break;
        }

        // RETR
        if (strncmp(command, RETR, 4) == 0)  {

            // Create data socket
            int data_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
            //Instance ID: 4-7
            if (data_sock_fd < 0) {
                perror("Socket creation failed");
                exit(EXIT_FAILURE);
            }

            // Form data connection to the server
            struct sockaddr_in server_data_addr;
            memset(&server_data_addr, 0, sizeof(server_data_addr));
            server_data_addr.sin_family = AF_INET;
            server_data_addr.sin_port = htons(cntrl_port+1);
            is_port_in_use(cntrl_port+1);
            if (inet_pton(AF_INET, server_data_address, &server_data_addr.sin_addr) <= 0) {
                perror("invalid server address");
                exit(EXIT_FAILURE);
            }
            if (connect(data_sock_fd, (struct sockaddr *)&server_cntrl_addr, sizeof(server_cntrl_addr)) < 0) {
                perror("connection failed");
                exit(EXIT_FAILURE);
            }

            printf("Enter the name of the file: ");
            scanf("%s", file);

            // Send a command to download the file
            sprintf(buffer, "RETR %s\r\n", file);
            send(cntrl_sock_fd, buffer, strlen(buffer), 0);
            receive_cmd(data_sock_fd, buffer, MAX_SIZE);
            printf("%s", buffer);


            // Open a file to write the data to
            //Instance ID: 8-1, preventing existing data from being overwritten
            int result = access(file, F_OK);
            if(result != 0) {
                FILE *fp = fopen(file, "wb");
            }
            if (fp == NULL) {
                perror("File opening failed");
                exit(EXIT_FAILURE);
            }

            // Read the data from the socket and write it to the file
            int bytes_received = 0;
            while ((bytes_received = recv(data_sock_fd, buffer, MAX_SIZE, 0)) > 0) {
                fwrite(buffer, 1, bytes_received, fp);
            }

            fclose(fp);
            close(data_sock_fd);

        }

        // PWD
        else if (strncmp(command, PWD, 3) == 0)  {
            // Send a command to download the file
            sprintf(buffer, "PWD\r\n");
            send(cntrl_sock_fd, buffer, strlen(buffer), 0);
            receive_cmd(cntrl_sock_fd, buffer, MAX_SIZE);
            printf("%s", buffer);
        }

        // STOR
        else if (strncmp(command, STOR, 4) == 0) {

        }

    }
}
