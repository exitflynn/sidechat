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

int quit = 0;
int sockfd = 0;
char name[100];

void prompt() {
  printf("> ");
  fflush(stdout);
}

void output_step2(char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // so that msg ends on hitting enter (on detecting \n)
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}


void exit_on_ctrl_c() {
    quit = 1;
}

void client_send_msg() { // the main part 
  char message[MSG_MAX_LEN] = {};
	char msg_with_username[MSG_MAX_LEN + 32] = {};

  while(1) {
  	prompt();
    fgets(message, MSG_MAX_LEN, stdin);
    output_step2(message, MSG_MAX_LEN);

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

void client_recv_msg() {
	char message[MSG_MAX_LEN] = {};
  while (1) {
		int receive = recv(sockfd, message, MSG_MAX_LEN, 0);
    if (receive > 0) {
      printf("%s", message);
      prompt();
    } 
    else if (receive == 0) {
			break;
    }
		memset(message, 0, sizeof(message));
  }
}

int client_func(){
    printf("Enter group id: ");
	int port;
	scanf("%d", &port);
	printf("Enter username: ");
    char name[100];
	scanf("%s", name);
	
	char *ip = "127.0.0.1"; // why use pointer here? so that changed permanently?

	// signal(SIGINT, exit_on_ctrl_c); // the signal handler here is used to catch ctrl-c in the rest of the program
	// SIGINT(errupt) is one of the predefined signals that's associated with terminal interrupt in linux (mostly ctrl-c)

	
	output_step2(name, strlen(name));

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
  if(pthread_create(&send_msg_thread, NULL, (void *) client_send_msg, NULL) != 0){
		printf("error in creating thread\n");
    return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
  if(pthread_create(&recv_msg_thread, NULL, (void *) client_recv_msg, NULL) != 0){
		printf("error in creating thread\n");
		return EXIT_FAILURE;
	}

	while (1){
		if(quit){
			printf("\nBye\n");
			break;
    }
	}

	close(sockfd);

	return EXIT_SUCCESS;
}

// =======================================================================
// Server Code now
// =======================================================================


#define MAX_CLIENT_NUMBER 100
#define OUTPUT_MAX_LEN 2048

int number_of_clients = 0;
int uid = 10;

/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;

client_t *clients[MAX_CLIENT_NUMBER];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Add clients to queue 
void server_queue_add(client_t *cl){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENT_NUMBER; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// remove clients to queue
void server_queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENT_NUMBER; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// send message from server to all clients except the one which sent it
void server_send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENT_NUMBER; ++i){
		if(clients[i]){
			if(clients[i]->uid != uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Handle all communication with the client
void *server_handle_client(void *arg){
	char output[OUTPUT_MAX_LEN];
	char name[32];
	int quit = 0;

	number_of_clients++;
	client_t *cli = (client_t *)arg;

	// Name
	if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){
		printf("Didn't enter the name.\n");
		quit = 1;
	} else{
		strcpy(cli->name, name);
		sprintf(output, "%s has joined\n", cli->name);
		printf("%s", output);
		server_send_message(output, cli->uid);
	}

	memset(output, 0, OUTPUT_MAX_LEN);

	while(1){
		if (quit) {
			break;
		}

		int receive = recv(cli->sockfd, output, OUTPUT_MAX_LEN, 0);
		if (receive > 0){
			if(strlen(output) > 0){
				server_send_message(output, cli->uid);

				output_step2(output, strlen(output));
				printf("%s\n", output);
			}
		} else if (receive == 0 || strcmp(output, "exit") == 0){
			sprintf(output, "%s has left\n", cli->name);
			printf("%s", output);
			server_send_message(output, cli->uid);
			quit = 1;
		} else {
			printf("ERROR: -1\n");
			quit = 1;
		}

		memset(output, 0, OUTPUT_MAX_LEN);
	}

  // remove client from queue and stop that client's thread
	close(cli->sockfd);
  server_queue_remove(cli->uid);
  free(cli);
  number_of_clients--;
  pthread_detach(pthread_self());

	return NULL;
}

int server_func(){
	printf("Enter grp id: ");
	int port;
	scanf("%d", &port);
	char *ip = "127.0.0.1";
	int option = 1;
	int listenfd = 0, connfd = 0;
  struct sockaddr_in serv_addr;
  struct sockaddr_in cli_addr;
  pthread_t tid;

  // Socket values
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(ip);
  serv_addr.sin_port = htons(port);

	/* Bind */
  bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	listen(listenfd, 10);

	printf("=== WELCOME TO THE CHATROOM ===\n");

	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		// client values
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;

		// add client to queue and make a new thread for it
		server_queue_add(cli);
		pthread_create(&tid, NULL, &server_handle_client, (void*)cli);

		// for less CPU usage
		sleep(1);
	}

	return EXIT_SUCCESS;
}



int main() {
    printf("1. Host Room\n2. Join Room\n> ");
    int ch;
    scanf("%d", &ch);
    if (ch==1) {
        server_func();
    }
    else if (ch==2) {
    printf("Enter group id: ");
	int port;
	scanf("%d", &port);
	printf("Enter username: ");
    // char name[100];
	scanf("%s", &name);
	
	char *ip = "127.0.0.1"; // why use pointer here? so that changed permanently?

	// signal(SIGINT, exit_on_ctrl_c); // the signal handler here is used to catch ctrl-c in the rest of the program
	// SIGINT(errupt) is one of the predefined signals that's associated with terminal interrupt in linux (mostly ctrl-c)

	
	output_step2(name, strlen(name));

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
		printf("there was an error in connecting to the server\n");
		return EXIT_FAILURE;
	}

	// Send name
	send(sockfd, name, 32, 0);
	printf("=== WELCOME TO THE CHATROOM ===\n");

	pthread_t send_msg_thread;
  if(pthread_create(&send_msg_thread, NULL, (void *) client_send_msg, NULL) != 0){
		printf("error in creating thread\n");
    return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
  if(pthread_create(&recv_msg_thread, NULL, (void *) client_recv_msg, NULL) != 0){
		printf("error in creating thread\n");
		return EXIT_FAILURE;
	}

	while (1){
		if(quit){
			printf("\nBye\n");
			break;
    }
	}

	close(sockfd);

	return EXIT_SUCCESS;
    }
    return 0;
}