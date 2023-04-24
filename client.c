#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define MAX_SIZE 1024


// WHERE THE FUCK IS THIS USED??
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
    
    cntrl_port = rand() % (49151 - 1024 + 1) + 1024; // Generate a random number between 1024 and 49151 (registered ports)

    return cntrl_port;
}

// Check to see if the port generated above is in use; if it is, try again
int is_port_in_use(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
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


int main() {
    // Get user input for server address, port, username, password, and file to download
    char server_cntrl_address[MAX_SIZE]; // For demo purposes, client and server are on the same machine and so use localhost addr
    char server_data_address [MAX_SIZE]; // For demo purposes, client and server are on the same machine and so use localhost addr
    int cntrl_port = port_number();
    char username[MAX_SIZE];
    char password[MAX_SIZE];
    char file[MAX_SIZE];
    char command[MAX_SIZE];
    char go_again[2];

    char RETR[] = "RETR";

    printf("Enter server address: ");
    scanf("%s", server_cntrl_address);
    //PORT is unnecessary here; it is assumed to use the default FTP control port of the server (21)
    printf("What would you like to do? ");
    scanf("%s", command);

    // Create control socket
    int cntrl_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (cntrl_sock_fd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Form control connection to the server
    struct sockaddr_in server_cntrl_addr;
    memset(&server_cntrl_addr, 0, sizeof(server_cntrl_addr));
    server_cntrl_addr.sin_family = AF_INET;
    server_cntrl_addr.sin_port = htons(cntrl_port);
    is_port_in_use(cntrl_port);
    if (inet_pton(AF_INET, server_cntrl_address, &server_cntrl_addr.sin_addr) <= 0) {
        perror("invalid server address");
        exit(EXIT_FAILURE);
    }
    if (connect(cntrl_sock_fd, (struct sockaddr *)&server_cntrl_addr, sizeof(server_cntrl_addr)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_SIZE];
/* PARTS OF THIS MAY BE NEEDED

    // Receive the server's welcome message
    char buffer[MAX_SIZE];
    receive_cmd(sock_fd, buffer, MAX_SIZE);
    printf("%s", buffer);
*/
    // Send a command to change the current directory
    sprintf(buffer, "CWD %s\r\n", file);
    send(cntrl_sock_fd, buffer, strlen(buffer), 0);
    receive_cmd(cntrl_sock_fd, buffer, MAX_SIZE);
    printf("%s", buffer);

if (strncmp(command, RETR, 4))
    printf("Enter the name of the file: ");
    scanf("%s", file);    
    // Send a command to download the file
    sprintf(buffer, "RETR %s\r\n", file);
    send(cntrl_sock_fd, buffer, strlen(buffer), 0);
    receive_cmd(cntrl_sock_fd, buffer, MAX_SIZE);
    printf("%s", buffer);

    // PRECEDE THIS WITH AN IF STATEMENT TO CHECK IF THE FILE EXISTS ON THE SERVER
    // Open a file to write the data to
    FILE *fp = fopen(file, "wb");
    if (fp == NULL) {
        perror("file opening failed");
        exit(EXIT_FAILURE);
    } 

    // CREATE DATA SOCKET HERE

     // Create data socket
    int data_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (data_sock_fd < 0) {
        perror("socket creation failed");
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

    // Read the data from the socket and write it to the file
    int bytes_received = 0;
    while ((bytes_received = recv(data_sock_fd, buffer, MAX_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, fp);
    }
    fclose(fp);



    printf("Would you like to do something else? y/n: ");
    scanf("%s", go_again);
    if (strncmp(go_again, "y", 2)) {
        return 0;
    }
    else if (strncmp(go_again, "n", 2)) {
        // Close the connections
        close(data_sock_fd);
        close(cntrl_sock_fd);
        printf("Connections closed! Have a bad day.");
        return 0;
    }
    else {
        printf("Usage: enter y or n");
    }
    return 0;

}
