  
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>

#include "server_functions.h"

int lport = 9999;
char* log_ip;

// Signal handler function to respond to Ctrl+C interrupt signal -- Enoch Ng
void handler(int s) { 
	printf("Received interrupt signal, shutting down");
	
	// Send a message to the log server now
	
	int sock, n;
	unsigned int length;
	struct sockaddr_in server, from;
	struct hostent *hp;
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		printf("Error creating socket");
		exit(1);
	}

	server.sin_family = AF_INET;
	hp = gethostbyname(log_ip);
	if (hp == 0) {
		printf("Error: unknown host");
		exit(1);
	}

	bcopy((char *)hp->h_addr, (char *)&server.sin_addr, hp->h_length);
	server.sin_port = htons(lport);
	length = sizeof(struct sockaddr_in);
	char* msg = "echo_s is stopping";
	n = sendto(sock, msg, strlen(msg), 0, (const struct sockaddr*) &server, length);
	if (n < 0) {
		printf("Error sending message");
		exit(1);
	}
	printf("Sent shutdown message to log server\n");
	close(sock);
	exit(0);
}

int run_serv(int port, char* log_ip) { // I moved most of the actual code in the main() function to this function -- Enoch
	struct serv *the_server = init_serv(port);
	if (!the_server) {
		printf("There was a problem starting the server. Hint: Double-check to make sure that you don't have multiple of the same port in your arguments.\n");
		error("Could not init server");
	}

	int pid = fork();
	if (pid == -1) {
		error("Could not create child process");
	}
	if (pid == 0) {
		if (tcp_proc(the_server, log_ip) == 1) {
			error("Error in handling TCP");
		}
	}
	else {
		if (udp_proc(the_server, log_ip) == 1) {
			error("Error in handling UDP");
		}
	}
	close_serv(the_server); 
	return 0;
}

//AUTH: Everyone pretty much

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Error: Need at least 1 port!\n");
		exit(1);
	}

	// Prepare arguments to be passed to init_serv -- Enoch Ng
	int ports[3];	
	int i;
	for (i = 0; i < argc - 1; i++) {
		ports[i] = atoi(argv[i + 1]);
		if(argc > i+2 && strcmp(argv[i+2], "-logip") == 0) {
			//we're done with ports if we hit the logip option
			//(manually i++ since breaking the loop won't execute the i++ above)
			i++;
			break;
		}
		if(i+1 > 3) {
			printf("No more than 3 ports allowed!\n");
			exit(1);
		}
	}
	int num_ports = i;
	printf("number of ports entered: %d\n", num_ports);

	if(argc >= num_ports + 2) {
		char* log_argv=argv[num_ports + 1];  //add some comment
		char* logip=argv[num_ports + 2];
		set_log_ip(logip);
		
		if(argc >= num_ports + 4) {
			char *logport_arg = argv[num_ports + 3];
			char *logport = argv[num_ports + 4];
			if(strcmp(logport_arg, "-logport") == 0) {
				//unsafe atoi
				set_log_port(atoi(logport));
			}
		}
	}

	log_ip = argv[i+2];  //add some comment
	
	// --------------------------------------
	// Signal handler
	struct sigaction sigIntHandler;
	memset(&sigIntHandler, 0, sizeof(sigIntHandler));
	sigIntHandler.sa_handler = handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	//sigset(SIGINT, handler);
	// --------------------------------------
	
	// Accept on multiple ports functionality - Enoch Ng
	// For the init_serv call, we'll fork the program 0-2 times (depending on the amount of ports), and call init_serv in each process
	// If the port limit were much higher, checking for every case with if-statements would be infeasible, but as it is, in the interest of time, I'm okay with just doing things the "brute force" way ...

	if (num_ports > 1) {
		int pid = fork();
		
		if (pid == 0) {
			run_serv(ports[0], log_ip);
		}

		else {
			if (num_ports > 2) {	
				int pid2 = fork();
				// I feel bad, but, not really
				if (pid2 == 0) {
					run_serv(ports[1], log_ip);
				}

				else {
					run_serv(ports[2], log_ip);
				}
			}

			else {
				// Only 2 ports
				run_serv(ports[1], log_ip);
			}
		}
	}

	else {
		// Only 1 port, hooray
		run_serv(ports[0], log_ip);
	}
	return 0;
}
