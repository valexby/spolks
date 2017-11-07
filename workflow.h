#ifndef WORKFLOW_H_   /* Include guard */
#define WORKFLOW_H_
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdbool.h>

#define FILE_PATH_UPLOAD "/home/valex/workspace/spolks/in/file.mp4"
#define FILE_PATH_DOWNLOAD "/home/valex/workspace/spolks/out/file.mp4"
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
bool checkCommand(char* command);
void closeSocket(SOCKET);
void printError(const char[]);
SOCKET setupKeepalive(SOCKET);
SOCKET createSocket(void);
bool validate(int value, int failed, SOCKET socket, const char func_name[]);
void init_sockaddr(struct sockaddr_in *sock, const char* ip);

int time_client(SOCKET socket);
int time_server(SOCKET socket);
int echo_client(SOCKET socket);
int echo_server(SOCKET socket, char* buffer);
int upload_client(SOCKET socket);
int upload_server(SOCKET socket);
int download_client(SOCKET socket);
int download_server(SOCKET socket);

int tcp_send(SOCKET sock, const char* buf, size_t len);
int tcp_recv(SOCKET sock, char* buf);

#endif // WORKFLOW_H_
