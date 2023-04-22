#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#define MAX_SIZE 1024

int main(int argc, char *argv[])
{
	// Check if the required port number is provided
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s<port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Parse the port number from the command-line argument
	int port = atoi(argv[1]);

	// Create a socket for listening
	int listen_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock_fd < 0)
	{
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	// Bind the socket to the specified port
	struct sockaddr_in server_addr = { 0 };

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	if (bind(listen_sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// Start listening on the socket
	if (listen(listen_sock_fd, 1) < 0)
	{
		perror("listen failed");
		exit(EXIT_FAILURE);
	}

	printf("FTP server listening on port %d...\n", port);

	while (1)
	{
		// Accept a connection from a client
		struct sockaddr_in client_addr = { 0 };

		socklen_t client_addr_len = sizeof(client_addr);
		int client_sock_fd = accept(listen_sock_fd, (struct sockaddr *) &client_addr, &client_addr_len);
		if (client_sock_fd < 0)
		{
			perror("accept failed");
			exit(EXIT_FAILURE);
		}

		printf("Client connected.\n");

		// Send a welcome message to the client
		char *welcome_msg = "220 Welcome to my FTP server.\r\n";
		send(client_sock_fd, welcome_msg, strlen(welcome_msg), 0);

		while (1)
		{
			// Receive a command from the client
			char buffer[MAX_SIZE] = { 0 };

			int bytes_received = recv(client_sock_fd, buffer, MAX_SIZE, 0);
			if (bytes_received < 0)
			{
				perror("recv failed");
				exit(EXIT_FAILURE);
			}

			// Parse the command
			char command[5] = { 0 };

			sscanf(buffer, "%4s", command);

			// Handle the command
			if (strcmp(command, "CWD") == 0)
			{
			 	// Parse the directory name from the command
				char dirname[MAX_SIZE] = { 0 };

				sscanf(buffer, "CWD %s\r\n", dirname);

				// Change the current directory to the specified directory
				if (chdir(dirname) < 0)
				{
				 		// Send an error response if the directory doesn't exist
					printf("%s\n", dirname);
					char *error_response = "550 Requested action not taken. Directory unavailable.\r\n";
					send(client_sock_fd, error_response, strlen(error_response), 0);
				}
				else
				{
				 		// Send a success response if the directory was changed
					char *success_response = "250 Directory changed.\r\n";
					send(client_sock_fd, success_response, strlen(success_response), 0);
				}
			}
			else if (strcmp(command, "RETR") == 0)
			{
			 	// Parse the file name from the command
				char filename[MAX_SIZE] = { 0 };

				sscanf(buffer, "RETR %s\r\n", filename);

				// Open the file for reading
				FILE *fp = fopen(filename, "rb");
				if (fp == NULL)
				{
				 		// Send an error response if the file doesn't exist
					char *error_response = "550 Requested action not taken. File unavailable.\r\n";
					send(client_sock_fd, error_response, strlen(error_response), 0);
				}
				else
				{
				 		// Send a success response
					char *success_response = "150 File status okay. About to open data connection.\r\n";
					send(client_sock_fd, success_response, strlen(success_response), 0);

					// Create a new socket for sending the file data
					int data_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
					if (data_sock_fd < 0)
					{
						perror("socket creation failed");
						exit(EXIT_FAILURE);
					}

					// Connect the data socket to the client's address
					if (connect(data_sock_fd, (struct sockaddr *) &client_addr, client_addr_len) < 0)
					{
						perror("connect failed");
						close(data_sock_fd);	// Close the data socket before exiting
						exit(EXIT_FAILURE);
					}

					// Bind the socket to the specified port
					struct sockaddr_in server_addr = { 0 };

					server_addr.sin_family = AF_INET;
					server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
					server_addr.sin_port = htons(port);

					if (bind(listen_sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
					{
						perror("bind failed");
						exit(EXIT_FAILURE);
					}

					// Start listening on the socket
					if (listen(listen_sock_fd, 1) < 0)
					{
						perror("listen failed");
						exit(EXIT_FAILURE);
					}

					// Send the file data to the client
					char data_buffer[MAX_SIZE] = { 0 };

					size_t bytes_read = 0;
					while ((bytes_read = fread(data_buffer, sizeof(char), MAX_SIZE, fp)) > 0)
					{
						if (send(data_sock_fd, data_buffer, bytes_read, 0) < 0)
						{
							perror("send failed");
							exit(EXIT_FAILURE);
						}
					}
				}
			}
			else if (strcmp(command, "QUIT") == 0)
			{
			 	// Send a response to the quit command
				char *quit_response = "221 Goodbye.\r\n";
				send(client_sock_fd, quit_response, strlen(quit_response), 0);
				break;	// exit the loop and close the connection
			}
			else
			{
			 	// Send an error response for unrecognized commands
				char *error_response = "500 Syntax error in FTP command.\r\n";
				send(client_sock_fd, error_response, strlen(error_response), 0);
			}
		}

		// Close the connection with the client
		close(client_sock_fd);
		printf("Client disconnected.\n");
	}

	// Close the listening socket
	close(listen_sock_fd);

	return 0;
}
