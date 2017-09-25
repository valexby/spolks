//#include <winsock2.h>
#include <cstdlib>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

typedef int SOCKET;
//#pragma comment(lib, "WS2_32.lib")

//#pragma warning(disable:4996)

#define PORT 27015
#define MESSAGE_MAX_SIZE 1024
#define COMMAND_LENGTH 128
#define COMMAND_SIZE 4
#define FILE_PATH "E:\\qwerty.mp4"
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

SOCKET configureServer(SOCKET &serverSock, char* ip);
SOCKET configureClient(char* ip);
void serverProccess(SOCKET serverSock, SOCKET &clientSock);
void clientProccess(SOCKET sock);
void printHelp();
char* getCurrentTime();
char* getSubstring(const char* string, int s, int l);
bool checkCommand(char* command);

typedef int SOCKET;

struct sockaddr_in lastClientSockAddr;

int main(int argc, char* argv[]) {
	SOCKET clientSock;

	if (!strcmp(argv[1], "server")) {
		SOCKET serverSock;
		if ((clientSock = configureServer(serverSock, argv[2])) != -1) {
			serverProccess(serverSock, clientSock);
			close(serverSock);
		}
	}
	else if (!strcmp(argv[1], "client")) {
		if ((clientSock = configureClient(argv[2])) != -1) {
			clientProccess(clientSock);
		}
	}

	close(clientSock);

	return 0;
}

SOCKET configureServer(SOCKET &serverSock, char* ip) {
	if ((serverSock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		printf("socket() error:%ld\n", strerror(errno));
		return -1;
	}
	printf("socket() success\n");

	struct sockaddr_in serverSockAddr;
	serverSockAddr.sin_family = AF_INET;
	serverSockAddr.sin_addr.s_addr = inet_addr(ip);
	serverSockAddr.sin_port = htons(PORT);

	if (bind(serverSock, (struct sockaddr*)&serverSockAddr, sizeof(serverSockAddr)) == SOCKET_ERROR) {
		printf("bind() error:%d\n", strerror(errno));
		close(serverSock);
		return -1;
	}
	printf("bind() success\n");

	if (listen(serverSock, 1) == SOCKET_ERROR) {
		printf("listen() error:%d\n", strerror(errno));
		close(serverSock);
		return -1;
	}
	printf("list() success\n");

	SOCKET connectedSock;
	struct sockaddr_in connectedSockAddr;
	socklen_t sockAddrLen = sizeof(struct sockaddr_in);

	printf("Waiting for connection\n");
	if ((connectedSock = accept(serverSock, (struct sockaddr*)&connectedSockAddr, &sockAddrLen)) == INVALID_SOCKET) {
		printf("accept() error:%d\n", strerror(errno));
		close(serverSock);
		return -1;
	}
	printf("Client(%s) connected\n", inet_ntoa(connectedSockAddr.sin_addr));
	lastClientSockAddr = connectedSockAddr;

	return connectedSock;
}

SOCKET configureClient(char* ip) {
	SOCKET clientSock;
	if ((clientSock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		printf("socket() error:%ld\n", strerror(errno));
		return -1;
	}
	printf("socket() success\n");

	struct sockaddr_in clientSockAddr;
	clientSockAddr.sin_family = AF_INET;
	clientSockAddr.sin_addr.s_addr = inet_addr(ip);
	clientSockAddr.sin_port = htons(PORT);

	printf("Connection to the server\n");
	if (connect(clientSock, (struct sockaddr*)&clientSockAddr, sizeof(clientSockAddr)) == SOCKET_ERROR) {
		printf("connect() error\n");
		close(clientSock);
		return -1;
	}
	printf("Connected\n");

	return clientSock;
}

void serverProccess(SOCKET serverSock, SOCKET &clientSock) {
	char buffer[MESSAGE_MAX_SIZE];
	bool disconnect = false;
	bool exit = false;
	bool canContinue = false;

	while (!exit) {
		int nowRecv = 0;

		if (disconnect) {
			struct sockaddr_in connectedSockAddr;
			socklen_t sockAddrLen = sizeof(struct sockaddr_in);

			printf("Waiting for connection\n");
			if ((clientSock = accept(serverSock, (struct sockaddr*)&connectedSockAddr, &sockAddrLen)) == INVALID_SOCKET) {
				printf("accept() error:%d\n", strerror(errno));
				close(clientSock);
				close(serverSock);
				return;
			}
			printf("Client(%s) connected\n", inet_ntoa(connectedSockAddr.sin_addr));
			disconnect = false;
			if (!strcmp(inet_ntoa(lastClientSockAddr.sin_addr), inet_ntoa(connectedSockAddr.sin_addr))) {
				printf("Hello\n");
				canContinue = true;
			}
			lastClientSockAddr = connectedSockAddr;
		}

		recv(clientSock, buffer, 4, 0);
		nowRecv = atoi(buffer);
		send(clientSock, "ok", 2, 0);

		nowRecv = recv(clientSock, buffer, nowRecv, 0);
		buffer[nowRecv] = '\0';

		if (!strcmp(getSubstring(buffer, 0, 4), "TIME")) {
			char* time = getCurrentTime();
			sprintf(buffer, "%d", strlen(time));
			send(clientSock, buffer, 4, 0);

			nowRecv = recv(clientSock, buffer, 2, 0);
			buffer[nowRecv] = '\0';
			if (!strcmp(buffer, "ok")) {
				send(clientSock, time, strlen(time), 0);
			}
		}
		else if (!strcmp(getSubstring(buffer, 0, 4), "ECHO")) {
			char* params = (char*)malloc(sizeof(char) * (strlen(buffer) - 5));
			memcpy(params, getSubstring(buffer, 5, strlen(buffer) - 5), strlen(buffer) - 5);

			sprintf(buffer, "%d", strlen(buffer) - 5);
			send(clientSock, buffer, 4, 0);

			nowRecv = recv(clientSock, buffer, 2, 0);
			buffer[nowRecv] = '\0';
			if (!strcmp(buffer, "ok")) {
				send(clientSock, params, strlen(params) - 5, 0);
			}
			free(params);
		}
		else if (!strcmp(getSubstring(buffer, 0, 5), "CLOSE")) {
			printf("Client(%s) disconnected\n", inet_ntoa(lastClientSockAddr.sin_addr));
			disconnect = true;
		}
		else if (!strcmp(buffer, "UPLOAD")) {
			nowRecv = recv(clientSock, buffer, 1, 0);
			buffer[nowRecv] = '\0';
			FILE *file = fopen("E:\\out.mp4", "a+b");
			if (canContinue) {
				fseek(file, 0, SEEK_END);
			}
			if (!strcmp(buffer, "!")) {
				sprintf(buffer, "%d", ftell(file));
				sprintf(buffer, "%d", strlen(buffer));
				send(clientSock, buffer, 4, 0);
				sprintf(buffer, "%d", ftell(file));
				send(clientSock, buffer, strlen(buffer), 0);

				do {
					nowRecv = recv(clientSock, buffer, sizeof(buffer), 0);
					if (nowRecv <= 0) {
						printf("Client(%s) disconnected\n", inet_ntoa(lastClientSockAddr.sin_addr));
						disconnect = true;
						canContinue = false;
						break;
					}
					if (nowRecv == 3 && !strcmp(getSubstring(buffer, 0, 3), "end")) {
						break;
					}
					fwrite(buffer, 1, nowRecv, file);
					send(clientSock, "ok", 2, 0);
				} while (1);
				fclose(file);
			}
		}
	}
}

void clientProccess(SOCKET sock) {
	bool exit = false;
	printHelp();

	while (!exit) {
		printf(">");
		char command[COMMAND_LENGTH];
		std::cin >> command;
		
		if (checkCommand(command)) {
			char buffer[MESSAGE_MAX_SIZE];
			bool disconnect = false;
			int remainSend = (int)strlen(command);
			int nowSend = 0, nowRecv = 0;

			sprintf(buffer, "%d", remainSend);
			send(sock, buffer, 4, 0);

			nowRecv = recv(sock, buffer, 2, 0);
			buffer[nowRecv] = '\0';

			if (!strcmp(buffer, "ok")) {
				sprintf(buffer, "%s", command);
				send(sock, buffer, remainSend, 0);

				if (!strcmp(command, "TIME")) {
					recv(sock, buffer, 4, 0);
					nowRecv = atoi(buffer);
					send(sock, "ok", 2, 0);
					nowRecv = recv(sock, buffer, nowRecv, 0);
					buffer[nowRecv] = '\0';
					printf("Server time:%s", buffer);
				}
				else if (!strcmp(getSubstring(command, 0, 4), "ECHO")) {
					recv(sock, buffer, 4, 0);
					nowRecv = atoi(buffer);
					send(sock, "ok", 2, 0);
					nowRecv = recv(sock, buffer, nowRecv, 0);
					buffer[nowRecv] = '\0';
					printf("ECHO result:%s\n", buffer);
					for (int i = 0; i < MESSAGE_MAX_SIZE; i++) {
						buffer[i] = '\0';
					}
				}
				else if (!strcmp(command, "CLOSE")) {
					exit = true;
				}
				else if (!strcmp(command, "UPLOAD")) {
					FILE *file = fopen(FILE_PATH, "r+b");
					send(sock, "!", 1, 0);
					recv(sock, buffer, 4, 0);
					nowRecv = atoi(buffer);
					recv(sock, buffer, nowRecv, 0);
					fseek(file, 0, SEEK_END);
					int size = ftell(file);
					fseek(file, atoi(buffer), SEEK_SET);
					int nowReaded = 0;
					int readed = atoi(buffer);
					while ((nowReaded = fread(buffer, 1, sizeof(buffer), file)) != 0) {
						readed += nowReaded;
						send(sock, buffer, nowReaded, 0);
						nowRecv = recv(sock, buffer, 2, 0);
						buffer[nowRecv] = '\0';
						if (strcmp(buffer, "ok")) {
							break;
						}
						printf("[%1.00f/100]\r", (float)(((float)readed / (float)size) * 100));
					}
					printf("\n");
					send(sock, "end", 3, 0);
					fclose(file);
				}
			}
		}
		else {
			printf("Wrong command\n");
		}
	}
}

void printHelp() {
	printf("Select the command:\n");
	printf("ECHO\n");
	printf("TIME\n");
	printf("CLOSE\n");
	printf("UPLOAD\n");
	printf("DOWNLOAD\n\n");
}

char* getCurrentTime() {
	time_t rawtime;
	struct tm* timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	return asctime(timeinfo);
}

char* getSubstring(const char* string, int s, int l) {
	char* substring = (char*)malloc(128);
	int c = 0;

	while (c < l) {
		substring[c] = string[s + c];
		c++;
	}
	substring[c] = '\0';

	return substring;
}

bool checkCommand(char* command) {
	return (!strcmp(command, "CLOSE") || !strcmp(command, "TIME") || !strcmp(getSubstring(command, 0, 4), "ECHO")
		|| !strcmp(command, "UPLOAD") || !strcmp(command, "DOWNLOAD"));
}
