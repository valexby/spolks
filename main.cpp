#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32

#include <winsock2.h>
#include <MSTcpIP.h>
#pragma comment(lib, "WS2_32.lib")
#pragma warning(disable:4996)
#define FILE_PATH_UPLOAD "C:\\qwerty.mp4"
#define FILE_PATH_DOWNLOAD "C:\\out.mp4"
typedef int socklen_t;

#endif

#ifdef __linux

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#define FILE_PATH_UPLOAD "/home/valex/workspace/spolks/in/file.mp4"
#define FILE_PATH_DOWNLOAD "/home/valex/workspace/spolks/out/file.mp4"
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
typedef int SOCKET;

#endif

#define PORT 27015
#define MESSAGE_MAX_SIZE 1024
#define COMMAND_LENGTH 128
#define COMMAND_SIZE 4

SOCKET configureServer(SOCKET &serverSock, char* ip);
SOCKET configureClient(char* ip);
void serverProccess(SOCKET serverSock, SOCKET &clientSock);
void clientProccess(SOCKET sock);
void printHelp();
char* getCurrentTime();
bool checkCommand(char* command);
void CloseSocket(SOCKET);
void PrintError(const char[]);

struct sockaddr_in lastClientSockAddr;

int main(int argc, char* argv[]) {
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	SOCKET clientSock;

	if (!strcmp(argv[1], "server")) {
		SOCKET serverSock;
		if ((clientSock = configureServer(serverSock, argv[2])) != -1) {
			serverProccess(serverSock, clientSock);
			CloseSocket(serverSock);
		}
	}
	else if (!strcmp(argv[1], "client")) {
		if ((clientSock = configureClient(argv[2])) != -1) {
			clientProccess(clientSock);
		}
		CloseSocket(clientSock);
	}

	return 0;
}	

SOCKET configureServer(SOCKET &serverSock, char* ip) {
	if ((serverSock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		PrintError("socket() error:");
		CloseSocket(serverSock);
		return -1;
	}
	printf("socket() success\n");

	struct sockaddr_in serverSockAddr;
	serverSockAddr.sin_family = AF_INET;
	serverSockAddr.sin_addr.s_addr = inet_addr(ip);
	serverSockAddr.sin_port = htons(PORT);

	if (bind(serverSock, (struct sockaddr*)&serverSockAddr, sizeof(serverSockAddr)) == SOCKET_ERROR) {
		PrintError("bind() error:");
		CloseSocket(serverSock);
		return -1;
	}
	printf("bind() success\n");

	if (listen(serverSock, 1) == SOCKET_ERROR) {
		PrintError("listen() error:");
		CloseSocket(serverSock);
		return -1;
	}
	printf("list() success\n");

	SOCKET connectedSock;
	struct sockaddr_in connectedSockAddr;
	socklen_t sockAddrLen = sizeof(struct sockaddr_in);

	printf("Waiting for connection\n");
	if ((connectedSock = accept(serverSock, (struct sockaddr*)&connectedSockAddr, &sockAddrLen)) == INVALID_SOCKET) {
		PrintError("accept() error:");
		CloseSocket(serverSock);
		return -1;
	}
	printf("Client(%s) connected\n", inet_ntoa(connectedSockAddr.sin_addr));
	lastClientSockAddr = connectedSockAddr;

#ifdef _WIN32
	DWORD dwBytesRet=0;  
	struct tcp_keepalive keepalive;
	keepalive.onoff = TRUE;
	keepalive.keepalivetime = 7200000;
	keepalive.keepaliveinterval = 1000;

	if(WSAIoctl(connectedSock, SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive), NULL, 0, &dwBytesRet, NULL, NULL) == SOCKET_ERROR) {
		PrintError("keepalive error:");
		CloseSocket(connectedSock);
		return -1;
	}
#endif

#ifdef __linux
	int optval = 1;
	socklen_t  optlen = sizeof(optval);

	if(setsockopt(connectedSock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) == SOCKET_ERROR) {
		PrintError("keepalive error:");
		CloseSocket(connectedSock);
		return -1;
	}
#endif

	return connectedSock;
}
	
SOCKET configureClient(char* ip) {
	SOCKET clientSock;
	if ((clientSock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		PrintError("socket() error:");
		CloseSocket(clientSock);
		return -1;
	}
	printf("socket() success\n");

	struct sockaddr_in clientSockAddr;
	clientSockAddr.sin_family = AF_INET;
	clientSockAddr.sin_addr.s_addr = inet_addr(ip);
	clientSockAddr.sin_port = htons(PORT);

	printf("Connection to the server\n");
	if (connect(clientSock, (struct sockaddr*)&clientSockAddr, sizeof(clientSockAddr)) == SOCKET_ERROR) {
		PrintError("connect() error:");
		CloseSocket(clientSock);
		return -1;
	}
	printf("Connected\n");

#ifdef _WIN32
	DWORD dwBytesRet=0;  
	struct tcp_keepalive keepalive;
	keepalive.onoff = TRUE;
	keepalive.keepalivetime = 7200000;
	keepalive.keepaliveinterval = 1000;

	if(WSAIoctl(connectedSock, SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive), NULL, 0, &dwBytesRet, NULL, NULL) == SOCKET_ERROR) {
		PrintError("keepalive error:");
		CloseSocket(connectedSock);
		return -1;
	}
#endif

#ifdef __linux
	int optval = 1;
	socklen_t  optlen = sizeof(optval);

	if(setsockopt(clientSock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) == SOCKET_ERROR) {
		PrintError("keepalive error:");
		CloseSocket(clientSock);
		return -1;
	}
#endif

	return clientSock;
}
	
void serverProccess(SOCKET serverSock, SOCKET &clientSock) {
	char buffer[MESSAGE_MAX_SIZE];
	bool disconnect = false;
	bool exit = false;
	bool canContinue = false;
	int lastPos = 0;

	while (!exit) {
		int nowRecv = 0;

		if (disconnect) {
			struct sockaddr_in connectedSockAddr;
			socklen_t sockAddrLen = sizeof(struct sockaddr_in);

			printf("Waiting for connection\n");
			if ((clientSock = accept(serverSock, (struct sockaddr*)&connectedSockAddr, &sockAddrLen)) == INVALID_SOCKET) {
				PrintError("accept() error:");
				CloseSocket(clientSock);
				CloseSocket(serverSock);
				return;
			}
			printf("Client(%s) connected\n", inet_ntoa(connectedSockAddr.sin_addr));
			if (!strcmp(inet_ntoa(connectedSockAddr.sin_addr), inet_ntoa(lastClientSockAddr.sin_addr))) {
				canContinue = true;
			}
			disconnect = false;
			lastClientSockAddr = connectedSockAddr;
		}

		if (recv(clientSock, buffer, 4, 0) > 0) {
			nowRecv = atoi(buffer);
			send(clientSock, "ok", 2, 0);

			nowRecv = recv(clientSock, buffer, nowRecv, 0);
			buffer[nowRecv] = '\0';

			if (!strncmp(buffer, "TIME", 4)) {
				printf("TIME command\n");
				char* time = getCurrentTime();
				sprintf(buffer, "%d", strlen(time));
				send(clientSock, buffer, 4, 0);

				nowRecv = recv(clientSock, buffer, 2, 0);
				buffer[nowRecv] = '\0';
				if (!strcmp(buffer, "ok")) {
					send(clientSock, time, strlen(time), 0);
				}
			}
			else if (!strncmp(buffer, "ECHO", 4)) {
				printf("%s command\n", buffer);
				char* params = (char*)malloc(sizeof(char) * (strlen(buffer) - 4));
				memcpy(params, buffer + 5, strlen(buffer) - 5);
				params[strlen(buffer) - 5] = '\0';

				sprintf(buffer, "%d", strlen(buffer) - 5);
				send(clientSock, buffer, 4, 0);

				nowRecv = recv(clientSock, buffer, 2, 0);
				buffer[nowRecv] = '\0';
				if (!strcmp(buffer, "ok")) {
					send(clientSock, params, strlen(params), 0);
				}
				free(params);
			}
			else if (!strncmp(buffer, "CLOSE", 5)) {
				printf("CLOSE command\n");
				printf("Client(%s) disconnected\n", inet_ntoa(lastClientSockAddr.sin_addr));
				disconnect = true;
			}
			else if (!strcmp(buffer, "UPLOAD")) {
				printf("UPLOAD command\n");
				nowRecv = recv(clientSock, buffer, 1, 0);
				buffer[nowRecv] = '\0';
				FILE *file = fopen(FILE_PATH_DOWNLOAD, "a+b");
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
						if (!strncmp(buffer, "end", 3)) {
							break;
						}
						fwrite(buffer, 1, nowRecv, file);
						send(clientSock, "ok", 2, 0);
					} while (1);
					fclose(file);
				}
			}

			else if (!strcmp(buffer, "DOWNLOAD")) {
				printf("DOWNLOAD command\n");
				FILE *file = fopen(FILE_PATH_UPLOAD, "r+b");
				fseek(file, 0, SEEK_END);
				sprintf(buffer, "%d", ftell(file));
				sprintf(buffer, "%d", strlen(buffer));
				send(clientSock, buffer, 4, 0);
				sprintf(buffer, "%d", ftell(file));
				send(clientSock, buffer, strlen(buffer), 0);
				if (canContinue) {
					canContinue = false;
				}
				else {
					lastPos = 0;
				}
				fseek(file, lastPos, SEEK_SET);
				sprintf(buffer, "%d", ftell(file));
				sprintf(buffer, "%d", strlen(buffer));
				send(clientSock, buffer, 4, 0);
				sprintf(buffer, "%d", ftell(file));
				send(clientSock, buffer, strlen(buffer), 0);
				int nowReaded = 0;
				while ((nowReaded = fread(buffer, 1, sizeof(buffer), file)) != 0) {
					send(clientSock, buffer, nowReaded, 0);
					nowRecv = recv(clientSock, buffer, 2, 0);
					if (nowRecv <= 0) {
						printf("Client(%s) disconnected\n", inet_ntoa(lastClientSockAddr.sin_addr));
						disconnect = true;
						canContinue = false;
						break;

					}
					if (!strncmp(buffer, "end", 3)) {
						break;
					}
					buffer[nowRecv] = '\0';
					if (strcmp(buffer, "ok")) {
						break;
					}
					lastPos = ftell(file);
				}
				if(!disconnect)
					send(clientSock, "end", 3, 0);
				fclose(file);
			}
		}
		else {
			printf("Client(%s) disconnected\n", inet_ntoa(lastClientSockAddr.sin_addr));
			disconnect = true;
		}
	}
}

void clientProccess(SOCKET sock) {
	bool exit = false;
	printHelp();

	while (!exit) {
		printf(">");
		char* command = (char*)malloc(COMMAND_LENGTH);
		fgets(command, COMMAND_LENGTH, stdin);
		command[strlen(command) - 1] = 0;

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
				else if (!strncmp(command, "ECHO", 4)) {
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
					FILE *file = fopen(FILE_PATH_UPLOAD, "r+b");
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
				else if (!strcmp(command, "DOWNLOAD")) {
					FILE *file = fopen(FILE_PATH_DOWNLOAD, "a+b");
					recv(sock, buffer, 4, 0);
					nowRecv = atoi(buffer);
					recv(sock, buffer, nowRecv, 0);
					int size = atoi(buffer);

					recv(sock, buffer, 4, 0);
					nowRecv = atoi(buffer);
					recv(sock, buffer, nowRecv, 0);
					fseek(file, 0, SEEK_END);
					fseek(file, atoi(buffer), SEEK_SET);
					int readed = atoi(buffer);
					do {
						nowRecv = recv(sock, buffer, sizeof(buffer), 0);
						if (!strncmp(buffer, "end", 3)) {
							break;
						}
						readed += nowRecv;
						fwrite(buffer, 1, nowRecv, file);
						send(sock, "ok", 2, 0);
						printf("[%1.00f/100]\r", (float)(((float)readed / (float)size) * 100));
					} while (1);
					printf("\n");
					fclose(file);
				}
			}
		}
		else {
			printf("Wrong command\n");
		}
		free(command);
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

bool checkCommand(char* command) {
	return (!strcmp(command, "CLOSE") || !strcmp(command, "TIME") || !strncmp(command, "ECHO", 4)
		|| !strcmp(command, "UPLOAD") || !strcmp(command, "DOWNLOAD"));
}

void CloseSocket(SOCKET socket) {
#ifdef _WIN32
	closesocket(socket);
	WSACleanup();
#endif

#ifdef __linux
	close(socket);
#endif
}

void PrintError(const char error_message[]) {
	printf(error_message);
#ifdef _WIN32
	printf("%ld\n", WSAGetLastError());
#endif

#ifdef __linux
		printf("%s\n", strerror(errno));
#endif
}
