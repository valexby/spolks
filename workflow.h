#ifndef WORKFLOW_H_   /* Include guard */
#define WORKFLOW_H_
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdbool.h>

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
typedef int SOCKET;

#define MESSAGE_MAX_SIZE 40960
#define COMMAND_LENGTH 128
#define COMMAND_SIZE 4
#define MAX_SEQ_NUMB 4
#define PORT 8888
#define LOGGING true

void printHelp();
char* getCurrentTime();
bool check_command(char* command);
void print_error(const char[]);
SOCKET setup_socket(SOCKET);
void init_sockaddr(struct sockaddr_in *sock, const char* ip);

int time_client(SOCKET socket);
int time_server(SOCKET socket);
int echo_client(SOCKET socket);
int echo_server(SOCKET socket, char* buffer);
int upload_client(SOCKET socket, char *filename);
int upload_server(SOCKET socket, char *filename);
int download_client(SOCKET socket, char *filename);
int download_server(SOCKET socket, char *filename);

int tcp_send(SOCKET sock, const char* buf, size_t len);
int tcp_recv(SOCKET sock, char* buf);

#endif // WORKFLOW_H_
