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
#define FILE_PATH_DOWNLOAD "E:\\out"
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
#define MESSAGE_MAX_SIZE 250
#define COMMAND_LENGTH 128
#define COMMAND_SIZE 4

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

int configureServer(SOCKET &serverSock, char* ip);
SOCKET configureClient(char* ip);
int serverListener(SOCKET serverSock);
void clientListener(SOCKET sock);
void tcpServer(SOCKET serverSock);
void udpServer(SOCKET serverSock);
void printHelp();
char* getCurrentTime();
bool checkCommand(char* command);
void closeSocket(SOCKET);
void printError(const char[]);
SOCKET setupKeepalive(SOCKET);
SOCKET createSocket(void);
bool validate(int value, int failed, SOCKET socket, const char func_name[]);
void init_sockaddr(sockaddr_in &sock, const char* ip);

void timeCommand(Type type, SOCKET socket);
void echoCommand(Type type, SOCKET socket);
int uploadCommand(Type type, SOCKET socket);
int downloadCommand(Type type, SOCKET socket);

SOCKET configureUDP(Type type, char* ip);
ssize_t _send(SOCKET sock, const char* buf, unsigned char len, Protocol protocol);
ssize_t _recv(SOCKET sock, char* buf, Protocol protocol);

const char OK_MSG[] = "ok";
const char END_MSG[] = "end";

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
	SOCKET serverSock;
	if (type == SERVER && protocol == TCP) {
		if ((configureServer(serverSock, argv[2])) != -1) {
			tcpServer(serverSock);
			closeSocket(serverSock);
		}
	}
	else if (type == CLIENT && protocol == TCP) {
		if ((clientSock = configureClient(argv[2])) != -1) {
			clientListener(clientSock);
		}
		closeSocket(clientSock);
	}
	else if (type == SERVER && protocol == UDP) {
		if ((serverSock = configureUDP(type, argv[2])) != -1) {
			udpServer(serverSock);
		}
		closeSocket(serverSock);
	}
	else {
		if ((clientSock = configureUDP(type, argv[2])) != -1) {
			clientListener(clientSock);
		}
		closeSocket(clientSock);
	}

	return 0;
}

int configureServer(SOCKET &serverSock, char* ip) {
	serverSock = createSocket();
    if (!validate(serverSock, -1, serverSock, "socket()")) {
		return -1;
	}

	sockaddr_in serverSockAddr;
	init_sockaddr(serverSockAddr, ip);

	int ret;
	ret = bind(serverSock, (struct sockaddr*)&serverSockAddr, sizeof(serverSockAddr));
	if (ret == SOCKET_ERROR) {
		printError("bind() error:");
		closeSocket(serverSock);
		return -1;
	}
	printf("bind() succes\n");

	ret = listen(serverSock, 1);
	if (ret == SOCKET_ERROR) {
		printError("listen() error:");
		closeSocket(serverSock);
		return -1;
	}
	printf("listen() succes\n");

	int t = 1;
	setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char*)t, sizeof(int));

	return 0;
}

SOCKET configureClient(char* ip) {
	SOCKET clientSock = createSocket();

	struct sockaddr_in clientSockAddr;
	init_sockaddr(clientSockAddr, ip);

	printf("Connection to the server\n");
	int ret = connect(clientSock, (struct sockaddr*)&clientSockAddr, sizeof(clientSockAddr));
	if (ret == SOCKET_ERROR) {
		printError("connect() error:");
		return -1;
	}
	printf("Connected\n");
	clientSock = setupKeepalive(clientSock);
	if (clientSock == -1) {
		printError("keepalive() error:");
		return -1;
	}

	return clientSock;
}

SOCKET configureUDP(Type type, char *ip) {
	SOCKET sock;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (!validate(sock, INVALID_SOCKET, sock, "socket")) {
		return -1;
	}
	
	if (type == SERVER) {
		sockaddr_in sockAddr;
		init_sockaddr(sockAddr, ip);
        int ret = bind(sock, (struct sockaddr*)&sockAddr, sizeof(sockAddr));
		if (!validate(ret, SOCKET_ERROR, sock, "bind")) {
			return -1;
		}
	}
	else {
		init_sockaddr(lastClientSockAddr, ip);
	}

	return sock;
}

void tcpServer(SOCKET serverSock) {
	bool exit = false;

	SOCKET connectedSock;
	struct sockaddr_in connectedSockAddr;
	socklen_t sockAddrLen = sizeof(struct sockaddr_in);
    connectedSock = accept(serverSock, (struct sockaddr*)&connectedSockAddr, &sockAddrLen);
	if (connectedSock == INVALID_SOCKET) {
		printError("accept() error:");
		closeSocket(serverSock);
		return;
	}
	printf("Client(%s) connected\n", inet_ntoa(connectedSockAddr.sin_addr));

	connectedSock = setupKeepalive(connectedSock);
	if (connectedSock == -1) {
		printError("keepalive() error:");
		closeSocket(serverSock);
        return;
	}
    while (!exit) {
        serverListener(connectedSock);
    }
}

void udpServer(SOCKET serverSock) {
	bool exit = false;

	while (!exit) {
		serverListener(serverSock);
	}
}

int serverListener(SOCKET sock) {
	int nowRecv;
    nowRecv = _recv(sock, buffer, protocol);
    buffer[nowRecv] = '\0';

    if (!strncmp(buffer, "TIME", 4)) {
        timeCommand(SERVER, sock);
        return 0;
    }
    else if (!strncmp(buffer, "ECHO", 4)) {
        echoCommand(SERVER, sock);
        return 0;
    }
    else if (!strncmp(buffer, "CLOSE", 5)) {
        printf("CLOSE command\n");
        printf("Client(%s) disconnected\n", inet_ntoa(lastClientSockAddr.sin_addr));
        return -1;
    }
    else if (!strcmp(buffer, "UPLOAD")) {
        if (uploadCommand(SERVER, sock) == -1) {
            return -1;
        }
        return 0;
    }

    else if (!strcmp(buffer, "DOWNLOAD")) {
        if (downloadCommand(SERVER, sock) == -1) {
            return -1;
        }
        return 0;
    }
	return 0;
}

void clientListener(SOCKET sock) {
	bool exit = false;
	printHelp();

	while (!exit) {
		printf(">");
		char* command = (char*)malloc(COMMAND_LENGTH);
		fgets(command, COMMAND_LENGTH, stdin);
		command[strlen(command) - 1] = 0;

		if (checkCommand(command)) {
            _send(sock, command, (unsigned char)strlen(command), protocol);

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
	if ((nooby = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
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

bool validate(int value, int failed, SOCKET nooby, const char func_name[]) {
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
        nowRecv = _recv(socket, buffer, protocol);
		buffer[nowRecv] = '\0';
		printf("Server time:%s", buffer);
	}
	else {
		printf("TIME command\n");
		char* time = getCurrentTime();
        _send(socket, time, (char)strlen(time), protocol);
	}
}

void echoCommand(Type type, SOCKET socket) {
	int nowRecv;
	if (type == CLIENT) {
		nowRecv = _recv(socket, buffer, protocol);
		buffer[nowRecv] = '\0';
		printf("ECHO result:%s\n", buffer);
		for (int i = 0; i < MESSAGE_MAX_SIZE; i++) {
			buffer[i] = '\0';
		}
	}
	else {
		printf("ECHO command\n");
        _send(socket, buffer + 5, strlen(buffer) - 5, protocol);
	}
}

int uploadCommand(Type type, SOCKET socket) {
	FILE *file;
	int nowRecv, size, nowReaded = 0, readed = 0;

	if (type == CLIENT) {
		file = fopen(FILE_PATH_UPLOAD, "r+b");
		fseek(file, 0, SEEK_END);
		size = ftell(file);
        fseek(file, 0, SEEK_SET);

		while ((nowReaded = fread(buffer, 1, sizeof(buffer), file)) != 0) {
			readed += nowReaded;
			_send(socket, buffer, nowReaded, protocol);
			//printf("[%1.00f/100]\r", (float)(((float)readed / (float)size) * 100));
		}
		printf("\n");
		_send(socket, END_MSG, 3, protocol);
	}
	else {
		file = fopen(FILE_PATH_DOWNLOAD, "wb");

		printf("UPLOAD command\n");
        nowRecv = _recv(socket, buffer, protocol);
        while (strncmp(buffer, END_MSG, 3))
        {
            if (nowRecv <= 0) {
                printf("Client(%s) disconnected\n", inet_ntoa(lastClientSockAddr.sin_addr));
                fclose(file);
                return -1;
            }
            fwrite(buffer, 1, nowRecv, file);
            nowRecv = _recv(socket, buffer, protocol);
        }
	}
    fclose(file);
    return 0;
}

int downloadCommand(Type type, SOCKET socket) {
	FILE *file;
	int nowRecv, size, readed, nowReaded = 0;
	if (type == CLIENT) {
		file = fopen(FILE_PATH_DOWNLOAD, "wb");

		_recv(socket, buffer, protocol);
		size = atoi(buffer);

        nowRecv = _recv(socket, buffer, protocol);
		while (strncmp(buffer, "end", 3)) {
			readed += nowRecv;
			fwrite(buffer, 1, nowRecv, file);
            nowRecv = _recv(socket, buffer, protocol);
			//printf("[%1.00f/100]\r", (float)(((float)readed / (float)size) * 100));
		}
		printf("\n");
	}
	else {
		printf("DOWNLOAD command\n");

		file = fopen(FILE_PATH_UPLOAD, "r+b");
		fseek(file, 0, SEEK_END);
		sprintf(buffer, "%ld", ftell(file));
		_send(socket, buffer, (unsigned char)strlen(buffer), protocol);
        fseek(file, 0, SEEK_SET);

		while ((nowReaded = fread(buffer, 1, sizeof(buffer), file)) != 0) {
			if (_send(socket, buffer, nowReaded, protocol) <= 0) {
				printf("Client(%s) disconnected\n", inet_ntoa(lastClientSockAddr.sin_addr));
				fclose(file);
				return -1;
			}
		}
		_send(socket, END_MSG, 3, protocol);
	}
    fclose(file);
    return 0;
}

ssize_t _send(SOCKET sock, const char* buf, unsigned char len, Protocol protocol) {
	int nowSend, nowRecv;
    char ok[2];
	if (protocol == TCP) {
        send(sock, &len, 1, MSG_OOB);
		nowSend = send(sock, buf, (size_t)len, 0);
        if (nowSend != (ssize_t)len) {
            return -1;
        }
        printf("%d bytes sent\n", nowSend);
        nowRecv = recv(sock, ok, 2, 0);
        printf("%d bytes received\n", nowRecv);
        if (strcmp(ok, OK_MSG)) {
            return -1;
        }
	}
	else {
		socklen_t slen = sizeof(lastClientSockAddr);
        sendto(sock, &len, 1, 0, (struct sockaddr*) &lastClientSockAddr, slen);
		nowSend = sendto(sock, buf, len, 0, (struct sockaddr*) &lastClientSockAddr, slen);
        if (nowSend != (ssize_t)len) {
            return -1;
        }
        printf("%d bytes sent\n", nowSend);
        nowRecv = recvfrom(sock, ok, 2, 0, (struct sockaddr*) &lastClientSockAddr, &slen);
        printf("%d bytes received\n", nowRecv);
    }
	return nowSend;
}

ssize_t _recv(SOCKET sock, char* buf, Protocol protocol) {
	int nowRecv, nowSend;
    unsigned char len = 0;
	if (protocol == TCP) {
        while (recv(sock, &len, 1, MSG_OOB) == -1) {};
		nowRecv = recv(sock, buf, (size_t)len, 0);
        if (nowRecv != (ssize_t)len) {
            return -1;
        }
        printf("%d bytes received\n", nowRecv);
        nowSend = send(sock, OK_MSG, 2, 0);
        printf("%d bytes sent\n", nowSend);
	}
	else {
		socklen_t slen;
		nowRecv = recvfrom(sock, &len, 1, 0, (struct sockaddr*) &lastClientSockAddr, &slen);
        printf("%d bytes received\n", nowRecv);
		nowRecv = recvfrom(sock, buf, len, 0, (struct sockaddr*) &lastClientSockAddr, &slen);
        printf("%d bytes received\n", nowRecv);
		nowSend = sendto(sock, OK_MSG, 2, 0, (struct sockaddr*) &lastClientSockAddr, slen);
        printf("%d bytes sent\n", nowSend);
	}

	return nowRecv;
}
