#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#define MAX_SIZE 1024
#define CNTRL_PORT "21"              // server-side control port
#define DATA_PORT 20                 // server-side data port
#define BACKLOG 10                   // how many pending connections will be held in queue

void sigchld_handler(int s) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
 {
    int status, cntrl_fd, new_fd, data_fd;  // various socket file descriptors needed
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_cntrl;
    struct sockaddr_storage client_data;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];

    char buffer[MAX_SIZE];
    char file[MAX_SIZE];

    char STOR[] = "STOR";
    char RETR[] = "RETR";
    char EXIT[] = "EXIT";

    memset(&hints, 0, sizeof(hints));       // ensure that hints is empty
    hints.ai_family = AF_UNSPEC;            // allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;        // send data as a stream rather than as a series of datagrams
    hints.ai_flags = AI_PASSIVE;            // tells getaddrinfo() to use local IP address

    if ((status = getaddrinfo(NULL, CNTRL_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        // Create cntrl_fd socket file descriptor
        if ((cntrl_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        // setsockopt to allow reuse of addr:port combinations 
        // NB: this is unsafe; don't do this in real life
        // we're doing it because client and server are on the same machine
        if (setsockopt(cntrl_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        // associate the file descriptor with a port
        if (bind(cntrl_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(cntrl_fd);
            perror("server: bind");
            continue;
        }

        break;
    } 

    // freeaddrinfo(servinfo); Might need this later, don't want to delete yet?

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(cntrl_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    // Collect and dispose of finished processes
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) { // main accept() loop
        sin_size = sizeof(client_cntrl);
        new_fd = accept(cntrl_fd, (struct sockaddr *)&client_cntrl, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(client_cntrl.ss_family, get_in_addr((struct sockaddr *)&client_cntrl), s, sizeof(s));
        printf("server: got new connection from %s\n", s);

        // ERROR HERE--probably to do with the way the command is sent from client
        /*
        if ((status = recv(cntrl_fd, buffer, sizeof(buffer), 0) < 0)) {
            perror("could not recv command");
            exit(1);            
        } */

        
            if ((status = recv(cntrl_fd, buffer, sizeof(buffer), 0) < 0)) {
                perror("could not recv command/file name");
                exit(1);            
            }
            else if ((strncmp(buffer, RETR, 4) == 0)) {  

        // Parse the file name from the command 
        char filename[MAX_SIZE] = {0};
        sscanf(buffer, "RETR %s\r\n", filename);

        printf("The requested file is: %s", filename);

        // Open the file for reading
        int result = access(filename, R_OK);
        if (result == 0) {        
            FILE *fp = fopen(filename, "rb");
            if (fp == NULL) {
                // Send an error response if the file doesn't exist
                char *error_response = "550 Requested action not taken. File unavailable.\r\n";
                send(cntrl_fd, error_response, strlen(error_response), 0);
        } 
        else {
            // Send a success response
            char *success_response = "150 File status okay. About to open data connection.\r\n";
            send(cntrl_fd, success_response, strlen(success_response), 0);

        
        }   
                       
        // Create data socket        
        data_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (data_fd < 0) {
            perror("socket creation failed");
            exit(EXIT_FAILURE);

        // Populate the sockaddr_in struct
        struct sockaddr_in server_data_addr = {0};
        server_data_addr.sin_family = AF_INET;
        server_data_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_data_addr.sin_port = htons(DATA_PORT);
        socklen_t server_data_addr_len = sizeof(server_data_addr);
        }

        // Connect the data socket to the client's address
        if (connect(data_fd, (struct sockaddr *)&servinfo, sizeof(servinfo) < 0)) {
            perror("connect failed");
            close(data_fd); 
            exit(EXIT_FAILURE);
        } 

        // Send the file data to the client
        char data_buffer[MAX_SIZE] = {0};
        size_t bytes_read = 0;
        while ((bytes_read = fread(data_buffer, sizeof(char), MAX_SIZE, fp)) > 0) {
            if (send(data_fd, buffer, bytes_read, 0) < 0) {
                perror("send failed");
                close(data_fd);
                exit(EXIT_FAILURE);
            }
        }  
        }

        close(data_fd);        
    } // RETR

}
 }