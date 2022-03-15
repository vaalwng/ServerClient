#include "server_functions.h"
#include <time.h>

int LOG_PORT = 9999;
void set_log_port(int log_port) {
	LOG_PORT = log_port;
}
const char *LOG_IP = "127.0.0.1";
void set_log_ip(const char *log_ip) {
	LOG_IP = log_ip;
}

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

void tcp_comm(int, struct serv *server, struct sockaddr_in cli_addr, char* log_ip);

struct serv *init_serv(int port) {
	// Note: the size of the ports array is always 3. But, the array might not always contain 3 ints (depending on how many arguments were supplied by the user).
	struct serv *new_serv = malloc(sizeof(struct serv));

	//create + bind + listen tcp socket
	int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(tcp_sock < 0) {
		printf("Error creating socket.\n");
		return NULL;
	}
	struct sockaddr_in server_addr;  
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(tcp_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		printf("Error binding socket for TCP.\n");
		return NULL;
	}
	listen(tcp_sock, 5);

	new_serv->tcp_fd = tcp_sock;

	// UDP server init, AUTH: Enoch Ng
	int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_sock < 0) {
		printf("Error creating socket.\n");
		return NULL;
	}

	if (bind(udp_sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
		printf("Error binding socket for UDP.\n");
		return NULL;
	}

	new_serv->udp_fd = udp_sock;

	return new_serv;
}


void close_serv(struct serv *server) {
	close(server->tcp_fd);
	close(server->udp_fd);
	free(server);
}


int tcp_proc(struct serv *server, char* log_ip)
{
	int sockfd, newsockfd, portno, pid;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	
	clilen = sizeof(cli_addr);
	//implement while loop: Jorge Macias
	while (1)
	{
		newsockfd = accept(server->tcp_fd, 
				(struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0) 
			error("ERROR on accept");
		//zombie process signal handler: Jorge Macias
		signal(SIGCHLD,SIG_IGN);
		//implement fork call: Jorge Macias
		pid = fork();
		if (pid < 0) 
			error("ERROR on fork");
		if (pid == 0)
		{
			close (server->tcp_fd);
			tcp_comm(newsockfd, server, cli_addr, log_ip); //call tcp_comm function to prepare for additional instance: Jorge Macias
			exit(0);
		}
		else close(newsockfd);
	} //end of while loop: Jorge Macias
	close(server->tcp_fd);
	return 0; 
}

int udp_proc(struct serv *server, char* log_ip) {
	char buf[1024];
	socklen_t clilen;
	struct sockaddr_in cli_addr;
	clilen = sizeof(cli_addr);

	char log_buf[1024];
	socklen_t log_len;
	struct sockaddr_in log_server;
	struct hostent *lp;
	log_server.sin_family = AF_INET;
	lp = gethostbyname(LOG_IP);	
	bcopy((char *)lp->h_addr, (char *) &log_server.sin_addr, lp->h_length);
	log_server.sin_port = htons(LOG_PORT);
	log_len = sizeof(log_server);
	
	while (1) {
		int n = recvfrom(server->udp_fd, buf, 1024, 0, (struct sockaddr*) &cli_addr, &clilen); 
		// n contains the number of bytes in the message
		if (n < 0) {
			error("Error reciving UDP message\n");
		}
		
		int pid = fork();
		if (pid == 0) {
			write(1, "Received a datagram: ", 21);
			write(1, buf, n);
			n = sendto(server->udp_fd, buf, n, 0, (struct sockaddr*) &cli_addr, clilen);
			if (n < 0) {
				error("Error in sending reply to UDP message\n");
			}
		}

		else { // Log server functionality, AUTH: Enoch Ng
			time_t rawtime;
			struct tm *timeinfo;
			time(&rawtime);
			timeinfo = localtime(&rawtime);
			strftime(log_buf, 1024, "%Y-%m-\%d %H:%M:\%S", timeinfo);

			char* address = inet_ntoa(cli_addr.sin_addr);
			strcat(log_buf, "\t\"");
			strcat(log_buf, buf);
			strcat(log_buf, "\" was received from ");
			strcat(log_buf, address);
			strcat(log_buf, "\n");
			// Add the contents of the message and other stuff here
			n = sendto(server->udp_fd, log_buf, strlen(log_buf), 0, (struct sockaddr*) &log_server, log_len); 
			if (n < 0) {
				error("Error in sending message to log server\n");
			}
			memset(log_buf, 0, 1024);
		}
	}

	return 0; // This should never happen ...
}

//AUTH: Jorge Macias
//Function to handle additional instances of client communication
void tcp_comm(int sock, struct serv *server, struct sockaddr_in cli_addr, char* log_ip)
{
	int a;
	char buff[256];

	bzero(buff, 256);
	a = read(sock, buff, 256);
	if (a < 0) error ("ERROR reading from socket");
	printf("Here is a message from somewhere: %s\n", buff);

	int pid = fork(); 
	if (pid == 0) {
		//a = write(sock, "Got some message from someplace", 18); // Keeping this commented in case I break something -- Enoch
		a = write(sock, buff, 256); // Echo server TCP implementation
		if (a < 0) error("ERROR writing from socket");
	}

	else { // Log server stuff, AUTH: Enoch Ng
		socklen_t log_len;
		struct sockaddr_in log_server;
		struct hostent *lp;
		log_server.sin_family = AF_INET;
		lp = gethostbyname(log_ip);
		bcopy((char *)lp->h_addr, (char *) &log_server.sin_addr, lp->h_length);
		log_server.sin_port = htons(LOG_PORT);
		log_len = sizeof(log_server);
	
		char log_buf[1024];
		char* address = inet_ntoa(cli_addr.sin_addr); // FIX THIS LATER

		time_t rawtime;
		struct tm *timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		strftime(log_buf, 1024, "%Y-%m-\%d %H:%M:\%S", timeinfo);

		strcat(log_buf, "\t\"");
		strcat(log_buf, buff);
		strcat(log_buf, "\" was received from ");
		strcat(log_buf, address);
		strcat(log_buf, "\n");
		a = sendto(server->udp_fd, log_buf, strlen(log_buf), 0, (struct sockaddr*) &log_server, log_len);
		memset(log_buf, 0, 1024);	

		if (a < 0) {
			error("Error in sending message to log server\n");
		}
	}
}
