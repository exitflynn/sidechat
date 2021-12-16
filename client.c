#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MSG_MAX_LEN 2048

// Global variables
// volatile sig_atomic_t exit = 0;
int exit = 0;
int sockfd = 0;
char name[32];

void str_overwrite_stdout() {
  printf("%s", "> ");
  fflush(stdout);
}

void normalise_fgets (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

void exit_on_ctrl_c() {
    exit = 1;
}

void send_msg() {
  char message[MSG_MAX_LEN] = {};
	char msg_with_username[MSG_MAX_LEN + 32] = {};

  while(1) {
  	str_overwrite_stdout();
    fgets(message, MSG_MAX_LEN, stdin);
    normalise_fgets(message, MSG_MAX_LEN);

    if (strcmp(message, "exit") == 0) {
			break;
    } else {
      sprintf(msg_with_username, "%s: %s\n", name, message);
      send(sockfd, msg_with_username, strlen(msg_with_username), 0);
    }

		memset(message, 0, MSG_MAX_LEN);
    memset(msg_with_username, 0, MSG_MAX_LEN + 32);
  }
  exit_on_ctrl_c();
}

void recv_msg() {
	char message[MSG_MAX_LEN] = {};
  while (1) {
		int receive = recv(sockfd, message, MSG_MAX_LEN, 0);
    if (receive > 0) {
      printf("%s", message);
      str_overwrite_stdout();
    } else if (receive == 0) {
			break;
    } else {
			// -1
		}
		memset(message, 0, sizeof(message));
  }
}

int main(){
	int port;
	printf("Enter Chatroom id (must be between 1024 and 49151): ");
	scanf("%d", &port);

	char *ip = "127.0.0.1"; // why use pointer here? so that changed permanently?

	signal(SIGINT, exit_on_ctrl_c); // the signal handler here is used to catch ctrl-c in the rest of the program
	// SIGINT(errupt) is one of the predefined signals that's associated with terminal interrupt in linux (mostly ctrl-c)

	printf("Enter username: ");
  fgets(name, 32, stdin);
  normalise_fgets(name, strlen(name));


	if (strlen(name) > 32 || strlen(name) < 2){
		printf("Name must be less than 30 and more than 2 characters.\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr;

	/* Socket settings */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port = htons(port);


  // Connect to Server
  int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (err == -1) {
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	// Send name
	send(sockfd, name, 32, 0);

	printf("=== WELCOME TO THE CHATROOM ===\n");

	pthread_t send_msg_thread;
  if(pthread_create(&send_msg_thread, NULL, (void *) send_msg, NULL) != 0){
		printf("ERROR: pthread\n");
    return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
  if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg, NULL) != 0){
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	while (1){
		if(exit){
			printf("\nBye\n");
			break;
    }
	}

	close(sockfd);

	return EXIT_SUCCESS;
}
