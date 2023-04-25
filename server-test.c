#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#define MAX_SIZE 1024
#define CNTRL_PORT 21
#define DATA_PORT 20

char getLocalAddr() {

    struct addrinfo hints, *res;
    int status;
    char ipstr[INET6_ADDRSTRLEN];

    // initialize the hints struct
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever is available
    hints.ai_socktype = SOCK_STREAM; // use TCP

    // get the address info for the localhost
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

int main () {

    int client_cntrl_sock_fd;
    char buffer[MAX_SIZE];
    char *response;

    // Used for strncmp purposes
    char RETR[] = "RETR";
    char CWD[] = "CWD";
    char STOR[] = "STOR";
    char LS[] = "ls";
    char PWD[] = "pwd";
    char EXIT[] = "exit";

    // Create a socket for listening
    int cntrl_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (cntrl_sock_fd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the control port (21)
    struct sockaddr_in server_cntrl_addr = {0};
    server_cntrl_addr.sin_family = AF_INET;
    server_cntrl_addr.sin_addr.s_addr = htonl(getLocalAddr());
    server_cntrl_addr.sin_port = htons(CNTRL_PORT);
    socklen_t server_cntrl_addr_len = sizeof(server_cntrl_addr);

    if (bind(cntrl_sock_fd, (struct sockaddr *)&server_cntrl_addr, sizeof(server_cntrl_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening on the socket 
    if (listen(cntrl_sock_fd, 1) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("FTP server listening on control port %d...\n", CNTRL_PORT);

    while (1) {

        // Accept a control connection from a client
       /* struct sockaddr_in client_cntrl_addr = {0};
        client_cntrl_addr.sin_family = AF_INET;
        client_cntrl_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        // client_cntrl_addr.sin_port = htons(client_cntrl_port); // BIND TO PROPER PORT -- might not be needed
        socklen_t client_cntrl_addr_len = sizeof(client_cntrl_addr); */

        if ((client_cntrl_sock_fd = accept(cntrl_sock_fd, (struct sockaddr *)&server_cntrl_addr, &server_cntrl_addr_len) < 0)) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        printf("Client connected.\n");

        // Send a welcome message to the client
        char *welcome_msg = "220 Welcome to my FTP server.\r\n";
        send(client_cntrl_sock_fd, welcome_msg, strlen(welcome_msg), 0);

        // Receive a command from the client
        int bytes_received = recv(client_cntrl_sock_fd, buffer, MAX_SIZE, 0);
        if (bytes_received < 0) {
        perror("Command could not be understood");
        exit(EXIT_FAILURE);
            }

        // Check what the command is and act accordingly

        if (strncmp(buffer, PWD, 3) == 0) {

            // Executes the command and dumps the result into response
            FILE *fp;
            fp = popen(buffer, "r");
            if (fp == NULL) {
                response = "Error executing command";
            }
            else {
                char temp[MAX_SIZE];
                memset (temp, 0, sizeof(temp));
                // Use fgets because it's probably less dangerous
                while (fgets(temp, sizeof(temp), fp) != NULL) {
                    strcat(buffer, temp);
                }
                pclose(fp);
                response = buffer;
            }

            // Sends the reponse (result of command execution) to the client over the control socket
            send(client_cntrl_sock_fd, response, strlen(response), 0);
            printf("Response sent: %s\n", response);
        }

        else if (strncmp(buffer, LS, 2) == 0) {
            
            // Executes the command and dumps the result into response
            FILE *fp;
            fp = popen(buffer, "r");
            if (fp == NULL) {
                response = "Error executing command";
            }
            else {
                char temp[MAX_SIZE];
                memset (temp, 0, sizeof(temp));
                // Use fgets because it's probably less dangerous
                while (fgets(temp, sizeof(temp), fp) != NULL) {
                    strcat(buffer, temp);
                }
                pclose(fp);
                response = buffer;
            }

            // Sends the reponse (result of command execution) to the client over the control socket
            send(client_cntrl_sock_fd, response, strlen(response), 0);
            printf("Response sent: %s\n", response);
            
        }   

        else if (strncmp(buffer, EXIT, 4) == 0) {

            close (client_cntrl_sock_fd); 
            close (cntrl_sock_fd);           
            
            response = "Connections closed on server";

            // Sends the reponse (result of command execution) to the client over the control socket
            send(client_cntrl_sock_fd, response, strlen(response), 0);
            printf("Response sent: %s\n", response);
            
        }       

    else if (strncmp(buffer, RETR, 4) == 0) {

        // Parse the file name from the command
        char filename[MAX_SIZE] = {0};
        sscanf(buffer, "RETR %s\r\n", filename);

        // Open the file for reading
        FILE *fp = fopen(filename, "rb");
        if (fp == NULL) {
            // Send an error response if the file doesn't exist
            char *error_response = "550 Requested action not taken. File unavailable.\r\n";
            send(cntrl_sock_fd, error_response, strlen(error_response), 0);
        } else {
            // Send a success response
            char *success_response = "150 File status okay. About to open data connection.\r\n";
            send(cntrl_sock_fd, success_response, strlen(success_response), 0);

        }

        int data_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (data_sock_fd < 0) {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Connect the data socket to the client's address
        if (connect(data_sock_fd, (struct sockaddr *)&client_cntrl_addr, client_cntrl_addr_len) < 0) {
            perror("connect failed");
            close(data_sock_fd); 
            exit(EXIT_FAILURE);
        }
    
        //Bind the socket to the data port
        struct sockaddr_in server_data_addr = {0};
        server_data_addr.sin_family = AF_INET;
        server_data_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_data_addr.sin_port = htons(DATA_PORT);

        if (bind(data_sock_fd, (struct sockaddr *)&server_data_addr, sizeof(server_data_addr)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }   

        // Send the file data to the client
        char data_buffer[MAX_SIZE] = {0};
        size_t bytes_read = 0;
        while ((bytes_read = fread(data_buffer, sizeof(char), MAX_SIZE, fp)) > 0) {
            if (send(data_sock_fd, data_buffer, bytes_read, 0) < 0) {
                perror("send failed");
                exit(EXIT_FAILURE);
            }
        }     



        // Executes the command and dumps the result into response
        FILE *fp;
        fp = popen(buffer, "r");
        if (fp == NULL) {
            response = "Error executing command";
        }
        else {
            char temp[MAX_SIZE];
            memset (temp, 0, sizeof(temp));
            // Use fgets because it's probably less dangerous
            while (fgets(temp, sizeof(temp), fp) != NULL) {
                strcat(buffer, temp);
            }
            pclose(fp);
            response = buffer;
        }

            // Sends the reponse (result of command execution) to the client over the control socket
            send(client_cntrl_sock_fd, response, strlen(response), 0);
            printf("Response sent: %s\n", response);

    } // while loop

} // main
}
