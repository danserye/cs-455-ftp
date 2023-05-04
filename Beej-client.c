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

#define PORT "21"
#define MAXDATASIZE 100

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return&(((struct sockaddr_in*)sa)->sin_addr);
    }
    else {
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
}

int main() {

    int client_cntrl_fd, client_data_fd, num_bytes;
    char buf[MAXDATASIZE];
    char data_buffer[MAXDATASIZE] = {0};
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    size_t bytes_read = 0;

    char file[MAXDATASIZE];
    char command[MAXDATASIZE];


    char STOR[] = "STOR";
    char RETR[] = "RETR";
    char EXIT[] = "EXIT";

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((client_cntrl_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(client_cntrl_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(client_cntrl_fd);
            perror("client: connect");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

// SOMETHING IS MUCHO WRONG HERE. THE COMMAND/FILE IS NOT BEING TAKEN IN CORRECTLY OR SENT OVER CNTRL
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
    printf("client: connecting to %s\n", s);
    printf("What would you like to do? RETR/STOR/EXIT <file_name>:\n");
    scanf("%s", command);
    printf("%s", command);

    if ((strncmp(command, RETR, 4) == 0)) {
        char *filename = command + 5;
        // printf("Enter the name of the file you wish to download: ");
        // scanf("%s", file);
        sscanf(buf, "RETR %s\r\n", filename);
        printf("%s", filename);        
        send(client_cntrl_fd, buf, strlen(buf), 0);
        // CHECK SERVER RETURN STATUS HERE

        FILE *fp = fopen(filename, "xb"); // open file for binary writing (allows non-.txt files); will fail if such a file already exists on the client
        if (fp == NULL) {
            perror("File opening failed (c)");
            exit(EXIT_FAILURE);
        }          
        // recv(client_data_fd, buf, sizeof(buf), 0);   MOVED INSIDE WHILE/IF BELOW

        while ((bytes_read = fread(data_buffer, sizeof(char), MAXDATASIZE, fp)) > 0) {
            if (recv(client_data_fd, buf, sizeof(buf), 0)) {
                perror("send failed");
                close(client_data_fd);
                exit(EXIT_FAILURE);
            }
        }
        
        fclose(fp);
        close(client_data_fd);
    }

    freeaddrinfo(servinfo);

    // at the end, close(client_cntrl_fd)
    close(client_cntrl_fd);

}