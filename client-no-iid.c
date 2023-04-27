#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#define MAX_SIZE 1024

int createDataSocket();

/* For illustrative purposes only; to simplify matters for demonstration, we use the localhost address for both client and server

char* getLocalAddr() {

    struct addrinfo hints, *res;
    int status;
    char ipstr[INET6_ADDRSTRLEN];

    // Initialize the hints struct
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever is available
    hints.ai_socktype = SOCK_STREAM; // use TCP

    // Get the address info for the localhost
    if ((status = getaddrinfo("localhost", NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // Loop through all the results and get the first IP address
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

        // Convert the IP address to a string and print it
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        printf("%s: %s\n", ipver, ipstr);
        char* localhost_addr = malloc(strlen(ipstr) + 1);        
        strncpy(localhost_addr, ipstr, strlen(ipstr));
        localhost_addr[strlen(ipstr)] = '\0';
        return localhost_addr;
        free(localhost_addr);
        break; // get the first IP address only
    }
}
*/

int enable_reuseaddr(int sockfd) {
    int optval = 1;
    return setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void receive_cmd(int sock_fd, char *buffer, size_t buffer_size) {
    int bytes_received = 0;
    while (bytes_received <= 0) {
        bytes_received = recv(sock_fd, buffer, buffer_size, 0);
    }
    buffer[bytes_received] = '\0';
}

// Generate a random number in the range of the registered TCP ports for use on the client
int port_number() {
    int cntrl_port;
    srand(time(NULL));  // Seed the random number generator with the current time
    
    cntrl_port = rand() % (49150 - 1024 + 1) + 1024; // Generate a random number between 1024 and 49150 (registered ports)

    return cntrl_port;
}


int main () {

    char server_cntrl_address[MAX_SIZE] = "127.0.0.1"; // For demo purposes, client and server are on the same machine and so use localhost addr
    char server_data_address[MAX_SIZE] = "127.0.0.1"; // For demo purposes, client and server are on the same machine and so use localhost addr
    char client_address[MAX_SIZE] = "127.0.0.1";
    int cntrl_port = port_number(); // We are dynamically generating the control port number on the client, 
                                    // so it might select a port that is already in use and complain ("Transport endpoint
                                    // is already connected" or something similar). If that happens, try again in a few seconds.
    char username[MAX_SIZE];
    char password[MAX_SIZE];
    char file[MAX_SIZE];
    char command[MAX_SIZE];
    char buffer[MAX_SIZE];

    // Used for strncmp and command wrapping purposes
    char PORT[] = "PORT";
    char RETR[] = "RETR";
    char CWD[] = "CWD";
    char STOR[] = "STOR";
    char LS[] = "ls";
    char PWD[] = "pwd";
    char EXIT[] = "EXIT";

/* You would ask the user for the server address if you weren't doing this all on one machine

    // Prompt user for server IP address
    printf("Enter server address: "); 
    scanf("%s", server_cntrl_address);

*/

// CONTROL SOCKET #####################################################################################################
    

    // Create control socket; socket() system call returns a file descriptor
    int cntrl_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (cntrl_sock_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Enable reuse of addr:port combinations; do NOT do this in real life
    if (enable_reuseaddr(cntrl_sock_fd) < 0) {
            perror("setsockopt");
            return 1;
        }

    // Bind the socket to the control port
    struct sockaddr_in client_cntrl_addr = {0};
    client_cntrl_addr.sin_family = AF_INET;
    client_cntrl_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_cntrl_addr.sin_port = htons(cntrl_port);
    socklen_t server_cntrl_addr_len = sizeof(client_cntrl_addr);

    if (bind(cntrl_sock_fd, (struct sockaddr *)&client_cntrl_addr, sizeof(client_cntrl_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening on the socket 
    if (listen(cntrl_sock_fd, 1) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

// CONTROL CONNECTION TO SERVER #####################################################################################################

    // Create socket
    int server_cntrl_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (cntrl_sock_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }    

    // Form control connection to the server
    struct sockaddr_in server_cntrl_addr;
    memset(&server_cntrl_addr, 0, sizeof(server_cntrl_addr));
    server_cntrl_addr.sin_family = AF_INET;
    server_cntrl_addr.sin_port = htons(cntrl_port);
    if (inet_pton(AF_INET, server_cntrl_address, &server_cntrl_addr.sin_addr) <= 0) {
        perror("Invalid server address");
        exit(EXIT_FAILURE);
    }
    if (connect(cntrl_sock_fd, (struct sockaddr *)&server_cntrl_addr, sizeof(server_cntrl_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Write the localhost address to the buffer and send to the server
    // snprintf(buffer, sizeof(buffer), "%s", PORT);
    snprintf(buffer, sizeof(buffer), "%s ", client_address); // In reality, you would use the result of getLocalAddress() here
    snprintf(buffer, sizeof(buffer), "%d", cntrl_port); 

    
    if (send(cntrl_sock_fd, buffer, strlen(buffer), 0) < 0) {
        printf("Error sending PORT\n");
        close(cntrl_sock_fd);
        exit(1);
    }    

    while (1) {

    if((cntrl_sock_fd = accept(server_cntrl_fd, (struct sockaddr *)&server_cntrl_addr, (socklen_t*)&server_cntrl_addr_len))<0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }  
                

    printf("What would you like to do? ");
    scanf("%s", command);

// COMMAND HANDLERS #####################################################################################################

// EXIT
    if (strncmp(command, EXIT, 4) == 0) {
        sprintf(buffer, "exit\r\n");
        send(cntrl_sock_fd, buffer, strlen(buffer), 0);
        close(cntrl_sock_fd);
        printf("Connections closed! Have a bad day.");
        break;
    }

// RETR
    if (strncmp(command, RETR, 4) == 0)  {

        // Create data socket
        int data_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (data_sock_fd < 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }  

        // Form data connection to the server
        struct sockaddr_in server_data_addr;
        memset(&server_data_addr, 0, sizeof(server_data_addr));
        server_data_addr.sin_family = AF_INET;
        server_data_addr.sin_port = htons(cntrl_port+1);
        if (inet_pton(AF_INET, server_data_address, &server_data_addr.sin_addr) <= 0) {
            perror("invalid server address");
            exit(EXIT_FAILURE);
        }
        if (connect(data_sock_fd, (struct sockaddr *)&server_cntrl_addr, sizeof(server_cntrl_addr)) < 0) {
            perror("connection failed");
            exit(EXIT_FAILURE);
        }    
      
        printf("Enter the name of the file to be downloaded: ");
        scanf("%s", file);
    
        // Send a command to download the file
        sprintf(buffer, "RETR %s\r\n", file);
        send(cntrl_sock_fd, buffer, strlen(buffer), 0);
        receive_cmd(data_sock_fd, buffer, MAX_SIZE);
        printf("%s", buffer);

                
        // Open a file to write the data to
        FILE *fp = fopen(file, "wb");
        if (fp == NULL) {
            perror("File opening failed");
            exit(EXIT_FAILURE);
        }  

        // Listen on the socket
        if (listen(data_sock_fd, 3) < 0) {
            perror("Could not listen");
            exit(EXIT_FAILURE);
        }        

        // Read the data from the socket and write it to the file
        int bytes_received = 0;
        while ((bytes_received = recv(data_sock_fd, buffer, MAX_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, fp);
        
        }
    
        // Close the file and the data connection
        fclose(fp);
        close(data_sock_fd);
        
    }

// PWD
    else if (strncmp(command, PWD, 3) == 0)  {

        // Send a command to the server
        sprintf(buffer, "pwd\r\n");
        send(cntrl_sock_fd, buffer, strlen(buffer), 0);
        receive_cmd(cntrl_sock_fd, buffer, MAX_SIZE);
        printf("%s", buffer);        

    }

// LS
    else if (strncmp(command, LS, 2) == 0)  {

        // Send a command to the server
        sprintf(buffer, "ls\r\n");
        send(cntrl_sock_fd, buffer, strlen(buffer), 0);
        receive_cmd(cntrl_sock_fd, buffer, MAX_SIZE);
        printf("%s", buffer);        

    }

// STOR
    else if (strncmp(command, STOR, 4) == 0) {
      
        printf("Enter the name of the file to be uploaded: ");
        scanf("%s", file);

        // Parse the file name from the command
        // char filename[MAX_SIZE] = {0};
        sscanf(buffer, "STOR %s\r\n", file);

        // Open the file for reading
        FILE *fp = fopen(file, "rb");
        if (fp == NULL) {
            // Send an error response if the file doesn't exist
            char *error_response = "550 Requested action not taken. File unavailable.\r\n";
            send(cntrl_sock_fd, error_response, strlen(error_response), 0);
        } else {
            // Send a success response
            char *success_response = "150 File status okay. About to open data connection.\r\n";
            send(cntrl_sock_fd, success_response, strlen(success_response), 0);

        }

        // Create data socket
        int data_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (data_sock_fd < 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }  

        // Form data connection to the server
        struct sockaddr_in server_data_addr;
        memset(&server_data_addr, 0, sizeof(server_data_addr));
        server_data_addr.sin_family = AF_INET;
        server_data_addr.sin_port = htons(cntrl_port+1);
        if (inet_pton(AF_INET, server_data_address, &server_data_addr.sin_addr) <= 0) {
            perror("invalid server address");
            exit(EXIT_FAILURE);
        }
        if (connect(data_sock_fd, (struct sockaddr *)&server_cntrl_addr, sizeof(server_cntrl_addr)) < 0) {
            perror("connection failed");
            exit(EXIT_FAILURE);
        }    
    
        // Send a command to upload the file
        sprintf(buffer, "STOR %s\r\n", file);
        send(cntrl_sock_fd, buffer, strlen(buffer), 0);
        receive_cmd(data_sock_fd, buffer, MAX_SIZE);
        printf("%s", buffer);

        // Send the file data to the server
        char data_buffer[MAX_SIZE] = {0};
        size_t bytes_read = 0;
        while ((bytes_read = fread(data_buffer, sizeof(char), MAX_SIZE, fp)) > 0) {
            if (send(data_sock_fd, data_buffer, bytes_read, 0) < 0) {
                perror("send failed");
                exit(EXIT_FAILURE);
            }
        }   

        close(data_sock_fd);  
    }
    }
}

