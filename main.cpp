#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32

#include <winsock2.h>
#include <MSTcpIP.h>
#pragma comment(lib, "WS2_32.lib")
#pragma warning(disable:4996)
#define FILE_PATH_UPLOAD "E:\\qwerty.mp4"
#define FILE_PATH_DOWNLOAD "E:\\out.mp4"
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
#define VALIDATE(value, falied, socket) __validate(value, falied, socket, __FUNCTION__)

enum Type {
	CLIENT, SERVER
};
enum Protocol {
	TCP, UDP
};

struct sockaddr_in lastClientSockAddr;
char buffer[MESSAGE_MAX_SIZE];
char oobBuf[1];
bool canContinue = false;
int lastPos = 0;
Protocol protocol = TCP;

SOCKET configureServer(SOCKET &serverSock, char* ip);
SOCKET configureClient(char* ip);
void serverProccess(SOCKET serverSock, SOCKET &clientSock);
void clientProccess(SOCKET sock);
void printHelp();
char* getCurrentTime();
bool checkCommand(char* command);
void closeSocket(SOCKET);
void printError(const char[]);
SOCKET setupKeepalive(SOCKET);
SOCKET createSocket(void);
bool __validate(int value, int failed, SOCKET socket, const char func_name[]);
void init_sockaddr(sockaddr_in &sock, const char* ip);

void timeCommand(Type type, SOCKET socket);
void echoCommand(Type type, SOCKET socket);
int uploadCommand(Type type, SOCKET socket);
int downloadCommand(Type type, SOCKET socket);

SOCKET configureUDP(Type type, char* ip);
int _send(SOCKET sock, char* buf, int len, Protocol protocol);
int _recv(SOCKET sock, char* buf, int len, Protocol protocol);

int main(int argc, char* argv[]) {
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	Type type = SERVER;
	if (!strcmp(argv[1], "server")) {
		type = SERVER;
	}
	else if (!strcmp(argv[1], "client")) {
		type = CLIENT;
	}

	if (argc == 4 && !strcmp(argv[3], "UDP")) {
		protocol = UDP;
	}
	else {
		protocol = TCP;
	}


	SOCKET clientSock;
	if (type == SERVER && protocol == TCP) {
		SOCKET serverSock;
		if ((clientSock = configureServer(serverSock, argv[2])) != -1) {
			serverProccess(serverSock, clientSock);
			closeSocket(serverSock);
		}
	}
	else if (type == CLIENT && protocol == TCP) {
		if ((clientSock = configureClient(argv[2])) != -1) {
			clientProccess(clientSock);
		}
		closeSocket(clientSock);
	}
	else if (type == SERVER && protocol == UDP) {
		if ((clientSock = configureUDP(type, argv[2])) != -1) {
			serverProccess(0, clientSock);
		}
	}
	else {
		if ((clientSock = configureUDP(type, argv[2])) != -1) {
			clientProccess(clientSock);
		}
	}

	return 0;
}

SOCKET configureServer(SOCKET &serverSock, char* ip) {
	serverSock = createSocket();

	sockaddr_in serverSockAddr;
	init_sockaddr(serverSockAddr, ip);

	int ret;
	ret = bind(serverSock, (struct sockaddr*)&serverSockAddr, sizeof(serverSockAddr));
	if (!VALIDATE(ret, SOCKET_ERROR, serverSock)) {
		return -1;
	}

	ret = listen(serverSock, 1);
	if (!VALIDATE(ret, SOCKET_ERROR, serverSock)) {
		return -1;
	}

	SOCKET connectedSock;
	struct sockaddr_in connectedSockAddr;
	socklen_t sockAddrLen = sizeof(struct sockaddr_in);

	printf("Waiting for connection\n");
	connectedSock = accept(serverSock, (struct sockaddr*)&connectedSockAddr, &sockAddrLen);
	if (!VALIDATE(connectedSock, INVALID_SOCKET, serverSock)) {
		return -1;
	}
	printf("Client(%s) connected\n", inet_ntoa(connectedSockAddr.sin_addr));
	lastClientSockAddr = connectedSockAddr;
	connectedSock = setupKeepalive(connectedSock);
	if (!VALIDATE(connectedSock, -1, connectedSock)) {
		return -1;
	}
	return connectedSock;
}

SOCKET configureClient(char* ip) {
	SOCKET clientSock = createSocket();

	struct sockaddr_in clientSockAddr;
	init_sockaddr(clientSockAddr, ip);

	printf("Connection to the server\n");
	int ret = connect(clientSock, (struct sockaddr*)&clientSockAddr, sizeof(clientSockAddr));
	if (!VALIDATE(ret, SOCKET_ERROR, clientSock)) {
		return -1;
	}
	printf("Connected\n");
	clientSock = setupKeepalive(clientSock);
	if (!VALIDATE(clientSock, -1, clientSock)) {
		return -1;
	}

	return clientSock;
}

SOCKET configureUDP(Type type, char *ip) {
	SOCKET sock;
	if (!VALIDATE((sock = socket(AF_INET, SOCK_DGRAM, 0)), INVALID_SOCKET, sock)) {
		return -1;
	}
	
	if (type == SERVER) {
		sockaddr_in sockAddr;
		init_sockaddr(sockAddr, ip);
		if (!VALIDATE(bind(sock, (struct sockaddr*)&sockAddr, sizeof(sockAddr)), SOCKET_ERROR, sock)) {
			return -1;
		}
	}
	else {
		init_sockaddr(lastClientSockAddr, ip);
	}

	return sock;
}

void serverProccess(SOCKET serverSock, SOCKET &clientSock) {
	bool disconnect = false;
	bool exit = false;

	while (!exit) {
		int nowRecv = 0;

		if (disconnect && protocol == TCP) {
			struct sockaddr_in connectedSockAddr;
			socklen_t sockAddrLen = sizeof(struct sockaddr_in);

			printf("Waiting for connection\n");
			if ((clientSock = accept(serverSock, (struct sockaddr*)&connectedSockAddr, &sockAddrLen)) == INVALID_SOCKET) {
				printError("accept() error:");
				closeSocket(clientSock);
				closeSocket(serverSock);
				return;
			}
			printf("Client(%s) connected\n", inet_ntoa(connectedSockAddr.sin_addr));
			if (!strcmp(inet_ntoa(connectedSockAddr.sin_addr), inet_ntoa(lastClientSockAddr.sin_addr))) {
				canContinue = true;
			}
			disconnect = false;
			lastClientSockAddr = connectedSockAddr;
		}

		if (_recv(clientSock, buffer, 4, protocol) > 0) {
			nowRecv = atoi(buffer);
			_send(clientSock, "ok", 2, protocol);

			nowRecv = _recv(clientSock, buffer, nowRecv, protocol);
			buffer[nowRecv] = '\0';

			if (!strncmp(buffer, "TIME", 4)) {
				timeCommand(SERVER, clientSock);
			}
			else if (!strncmp(buffer, "ECHO", 4)) {
				echoCommand(SERVER, clientSock);
			}
			else if (!strncmp(buffer, "CLOSE", 5)) {
				printf("CLOSE command\n");
				printf("Client(%s) disconnected\n", inet_ntoa(lastClientSockAddr.sin_addr));
				disconnect = true;
			}
			else if (!strcmp(buffer, "UPLOAD")) {
				if (uploadCommand(SERVER, clientSock) == -1) {
					disconnect = true;
					canContinue = false;
				}
			}

			else if (!strcmp(buffer, "DOWNLOAD")) {
				if (downloadCommand(SERVER, clientSock) == -1) {
					disconnect = true;
					canContinue = false;
				}
				if (!disconnect) {
					_send(clientSock, "end", 3, protocol);
				}
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
			bool disconnect = false;
			int remainSend = (int)strlen(command);
			int nowSend = 0, nowRecv = 0;

			sprintf(buffer, "%d", remainSend);
			_send(sock, buffer, 4, protocol);

			nowRecv = _recv(sock, buffer, 2, protocol);
			buffer[nowRecv] = '\0';

			if (!strcmp(buffer, "ok")) {
				sprintf(buffer, "%s", command);
				_send(sock, buffer, remainSend, protocol);

				if (!strcmp(command, "TIME")) {
					timeCommand(CLIENT, sock);
				}
				else if (!strncmp(command, "ECHO", 4)) {
					echoCommand(CLIENT, sock);
				}
				else if (!strcmp(command, "CLOSE")) {
					exit = true;
				}
				else if (!strcmp(command, "UPLOAD")) {
					uploadCommand(CLIENT, sock);
				}
				else if (!strcmp(command, "DOWNLOAD")) {
					downloadCommand(CLIENT, sock);
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

SOCKET setupKeepalive(SOCKET socket) {
#ifdef _WIN32
	DWORD dwBytesRet = 0;
	struct tcp_keepalive keepalive;
	keepalive.onoff = TRUE;
	keepalive.keepalivetime = 7200000;
	keepalive.keepaliveinterval = 1000;

	if (WSAIoctl(socket, SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive), NULL, 0, &dwBytesRet, NULL, NULL) == SOCKET_ERROR) {
		return -1;
	}
#endif

#ifdef __linux
	int optval = 1;
	socklen_t  optlen = sizeof(optval);

	if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) == SOCKET_ERROR) {
		return -1;
	}
#endif

	return socket;
}

SOCKET createSocket(void) {
	SOCKET nooby;
	if (!VALIDATE((nooby = socket(AF_INET, SOCK_STREAM, 0)), INVALID_SOCKET, nooby)) {
		return -1;
	}
	return nooby;
}

void closeSocket(SOCKET nooby) {
#ifdef _WIN32
	closeSocket(nooby);
	WSACleanup();
#endif

#ifdef __linux
	close(nooby);
#endif
}

void printError(const char error_message[]) {
	printf("%s", error_message);
#ifdef _WIN32
	printf("%ld\n", WSAGetLastError());
#endif

#ifdef __linux
	printf("%s\n", strerror(errno));
#endif
}

bool __validate(int value, int failed, SOCKET nooby, const char func_name[]) {
	char* msg = (char*)malloc(strlen(func_name) + 8);
	memcpy(msg, func_name, strlen(func_name));
	if (value == failed) {
		msg[strlen(func_name) + 7] = 0;
		printError(strcat(msg, " error:"));
		closeSocket(nooby);
		return false;
	}

	msg[strlen(func_name)] = 0;
	printf("%s success\n", msg);
	return true;
}

void init_sockaddr(sockaddr_in &sock, const char* ip) {
	sock.sin_family = AF_INET;
	sock.sin_addr.s_addr = inet_addr(ip);
	sock.sin_port = htons(PORT);
}

void timeCommand(Type type, SOCKET socket) {
	int nowRecv;
	if (type == CLIENT) {
		_recv(socket, buffer, 4, protocol);
		nowRecv = atoi(buffer);
		_send(socket, "ok", 2, protocol);
		nowRecv = _recv(socket, buffer, nowRecv, protocol);
		buffer[nowRecv] = '\0';
		printf("Server time:%s", buffer);
	}
	else {
		printf("TIME command\n");
		char* time = getCurrentTime();
		sprintf(buffer, "%ld", strlen(time));
		_send(socket, buffer, 4, protocol);

		nowRecv = _recv(socket, buffer, 2, protocol);
		buffer[nowRecv] = '\0';
		if (!strcmp(buffer, "ok")) {
			_send(socket, time, strlen(time), protocol);
		}
	}
}

void echoCommand(Type type, SOCKET socket) {
	int nowRecv;
	if (type == CLIENT) {
		_recv(socket, buffer, 4, protocol);
		nowRecv = atoi(buffer);
		_send(socket, "ok", 2, protocol);
		nowRecv = _recv(socket, buffer, nowRecv, protocol);
		buffer[nowRecv] = '\0';
		printf("ECHO result:%s\n", buffer);
		for (int i = 0; i < MESSAGE_MAX_SIZE; i++) {
			buffer[i] = '\0';
		}
	}
	else {
		printf("ECHO command\n", buffer);
		char* params = (char*)malloc(sizeof(char) * (strlen(buffer) - 4));
		memcpy(params, buffer + 5, strlen(buffer) - 5);
		params[strlen(buffer) - 5] = '\0';

		sprintf(buffer, "%ld", strlen(buffer) - 5);
		_send(socket, buffer, 4, protocol);

		nowRecv = _recv(socket, buffer, 2, protocol);
		buffer[nowRecv] = '\0';
		if (!strcmp(buffer, "ok")) {
			_send(socket, params, strlen(params), protocol);
		}
		free(params);
	}
}

int uploadCommand(Type type, SOCKET socket) {
	FILE *file;
	int nowRecv, size, nowReaded = 0, readed;

	if (type == CLIENT) {
		file = fopen(FILE_PATH_UPLOAD, "r+b");

		_send(socket, "!", 1, protocol);
		_recv(socket, buffer, 4, protocol);
		nowRecv = atoi(buffer);
		_recv(socket, buffer, nowRecv, protocol);
		fseek(file, 0, SEEK_END);
		size = ftell(file);
		fseek(file, atoi(buffer), SEEK_SET);
		readed = atoi(buffer);

		while ((nowReaded = fread(buffer, 1, sizeof(buffer), file)) != 0) {
			readed += nowReaded;
			_send(socket, buffer, nowReaded, protocol);
			nowRecv = _recv(socket, buffer, 2, protocol);
			buffer[nowRecv] = '\0';
			if (strcmp(buffer, "ok")) {
				break;
			}
			//printf("[%1.00f/100]\r", (float)(((float)readed / (float)size) * 100));
		}
		printf("\n");
		_send(socket, "end", 3, protocol);
		fclose(file);

		return 0;
	}
	else {
		file = fopen(FILE_PATH_DOWNLOAD, "a+b");
		if (canContinue) {
			fseek(file, 0, SEEK_END);
		}

		printf("UPLOAD command\n");
		nowRecv = recv(socket, buffer, 1, 0);
		buffer[nowRecv] = '\0';
		if (!strcmp(buffer, "!")) {
			sprintf(buffer, "%d", ftell(file));
			sprintf(buffer, "%d", strlen(buffer));
			send(socket, buffer, 4, 0);
			sprintf(buffer, "%d", ftell(file));
			send(socket, buffer, strlen(buffer), 0);
			do {
				nowRecv = recv(socket, buffer, sizeof(buffer), 0);
				if (nowRecv <= 0) {
					printf("Client(%s) disconnected\n", inet_ntoa(lastClientSockAddr.sin_addr));
					fclose(file);
					return -1;
				}
				if (!strncmp(buffer, "end", 3)) {
					break;
				}
				fwrite(buffer, 1, nowRecv, file);
				send(socket, "ok", 2, 0);
			} while (1);
			fclose(file);
		}

		return 0;
	}
}

int downloadCommand(Type type, SOCKET socket) {
	FILE *file;
	int nowRecv, size, readed, nowReaded = 0;

	if (type == CLIENT) {
		file = fopen(FILE_PATH_DOWNLOAD, "a+b");

		_recv(socket, buffer, 4, protocol);
		nowRecv = atoi(buffer);
		_recv(socket, buffer, nowRecv, protocol);
		size = atoi(buffer);
		_recv(socket, buffer, 4, protocol);
		nowRecv = atoi(buffer);
		_recv(socket, buffer, nowRecv, protocol);
		readed = atoi(buffer);

		fseek(file, 0, SEEK_END);
		fseek(file, atoi(buffer), SEEK_SET);
		do {
			nowRecv = _recv(socket, buffer, sizeof(buffer), protocol);
			if (!strncmp(buffer, "end", 3)) {
				break;
			}
			readed += nowRecv;
			fwrite(buffer, 1, nowRecv, file);
			_send(socket, "ok", 2, protocol);
			//printf("[%1.00f/100]\r", (float)(((float)readed / (float)size) * 100));
		} while (1);
		printf("\n");
		fclose(file);

		return 0;
	}
	else {
		printf("DOWNLOAD command\n");

		file = fopen(FILE_PATH_UPLOAD, "r+b");
		fseek(file, 0, SEEK_END);
		sprintf(buffer, "%d", ftell(file));
		sprintf(buffer, "%d", strlen(buffer));
		_send(socket, buffer, 4, protocol);
		sprintf(buffer, "%d", ftell(file));
		_send(socket, buffer, strlen(buffer), protocol);

		if (canContinue) {
			canContinue = false;
		}
		else {
			lastPos = 0;
		}
		fseek(file, lastPos, SEEK_SET);
		sprintf(buffer, "%d", ftell(file));
		sprintf(buffer, "%d", strlen(buffer));
		_send(socket, buffer, 4, protocol);
		sprintf(buffer, "%d", ftell(file));
		_send(socket, buffer, strlen(buffer), protocol);

		while ((nowReaded = fread(buffer, 1, sizeof(buffer), file)) != 0) {
			_send(socket, buffer, nowReaded, protocol);
			nowRecv = _recv(socket, buffer, 2, protocol);
			if (nowRecv <= 0) {
				printf("Client(%s) disconnected\n", inet_ntoa(lastClientSockAddr.sin_addr));
				fclose(file);
				return -1;

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

		fclose(file);

		return 0;
	}
}

int _send(SOCKET sock, char* buf, int len, Protocol protocol) {
	int nowSend = 0;
	if (protocol == TCP) {
		nowSend = send(sock, buf, len, 0);
		if (send(sock, "^", 1, MSG_OOB) > 0) {
			printf("%d bytes sent\n", nowSend);
		}
	}
	else {
		nowSend = sendto(sock, buf, len, 0, (struct sockaddr*) &lastClientSockAddr, sizeof(lastClientSockAddr));
	}

	return nowSend;
}

int _recv(SOCKET sock, char* buf, int len, Protocol protocol) {
	int nowRecv = 0;
	if (protocol == TCP) {
		nowRecv = recv(sock, buf, len, 0);
		if (recv(sock, oobBuf, 1, MSG_OOB) > 0 && !strcmp(oobBuf, "^")) {
			printf("%d bytes received\n", nowRecv);
		}
	}
	else {
		int slen = sizeof(lastClientSockAddr);
		nowRecv = recvfrom(sock, buf, len, 0, (struct sockaddr*) &lastClientSockAddr, &slen);
	}

	return nowRecv;
}