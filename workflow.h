#ifndef WORKFLOW_H_   /* Include guard */
#define WORKFLOW_H_
#include <netinet/in.h>

#define FILE_PATH_UPLOAD "/home/valex/workspace/spolks/in/lab"
#define FILE_PATH_DOWNLOAD "/home/valex/workspace/spolks/out/lab"
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
typedef int SOCKET;

#define MESSAGE_MAX_SIZE 250
#define COMMAND_LENGTH 128
#define COMMAND_SIZE 4
#define MAX_SEQ_NUMB 4
#define PORT 8888
#define LOGGING true
enum Type {
	CLIENT, SERVER
};
enum Protocol {
	TCP, UDP
};

struct Context {
	sockaddr_in *sock_addr;
	unsigned char *seq_number = 0;
	Protocol protocol = TCP;
};
const char OK_MSG[] = "ok";
const char END_MSG[] = "end";

void printHelp();
char* getCurrentTime();
bool checkCommand(char* command);
void closeSocket(SOCKET);
void printError(const char[]);
SOCKET setupKeepalive(SOCKET);
SOCKET createSocket(void);
bool validate(int value, int failed, SOCKET socket, const char func_name[]);
void init_sockaddr(sockaddr_in *sock, const char* ip);

int time_client(SOCKET socket, Context);
int time_server(SOCKET socket, Context);
int echo_client(SOCKET socket, Context);
int echo_server(SOCKET socket, Context);
int upload_client(SOCKET socket, Context);
int upload_server(SOCKET socket, Context);
int download_client(SOCKET socket, Context);
int download_server(SOCKET socket, Context);

int udp_send(SOCKET sock, const char* buf, size_t len, Context);
int tcp_send(SOCKET sock, const char* buf, size_t len);
int udp_recv(SOCKET sock, char* buf, Context);
int tcp_recv(SOCKET sock, char* buf);
int _recv(SOCKET sock, char* buf, Context);
int _send(SOCKET sock, const char* buf, size_t len, Context);
void inc_seq_numb(unsigned char *current_seq_numb);
int comp_seq_numb(unsigned char first, unsigned char second);

#endif // WORKFLOW_H_
