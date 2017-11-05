#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include "workflow.h"

int configureServer(SOCKET &server_sock, char* ip);
SOCKET configureClient(char* ip);
int serverListener(SOCKET server_sock, Context);
SOCKET configureUDP(Type type, char *ip, Context);
void clientListener(SOCKET sock, Context);
void tcpServer(SOCKET server_sock);
void udpServer(SOCKET server_sock);

int main(int argc, char* argv[]) {
	Type type = SERVER;
    Context context;
	SOCKET clientSock;
	SOCKET server_sock;
    context.sock_addr = (sockaddr_in*)malloc(sizeof(sockaddr_in));
    context.seq_number = (unsigned char*)malloc(sizeof(unsigned char*));
    *context.seq_number = 0;
	if (!strcmp(argv[1], "server")) {
		type = SERVER;
	}
	else if (!strcmp(argv[1], "client")) {
		type = CLIENT;
	}

	if (argc == 4 && !strcmp(argv[3], "UDP")) {
		context.protocol = UDP;
	}
	else {
		context.protocol = TCP;
	}
	if (type == SERVER && context.protocol == TCP) {
		if ((configureServer(server_sock, argv[2])) != -1) {
			tcpServer(server_sock);
			closeSocket(server_sock);
		}
	}
	else if (type == CLIENT && context.protocol == TCP) {
		if ((clientSock = configureClient(argv[2])) != -1) {
			clientListener(clientSock, context);
		}
		closeSocket(clientSock);
	}
	else if (type == SERVER && context.protocol == UDP) {
		if ((server_sock = configureUDP(type, argv[2], context)) != -1) {
			udpServer(server_sock);
		}
		closeSocket(server_sock);
	}
	else {
		if ((clientSock = configureUDP(type, argv[2], context)) != -1) {
			clientListener(clientSock, context);
		}
		closeSocket(clientSock);
	}
    free(context.sock_addr);
    free(context.seq_number);
	return 0;
}

int configureServer(SOCKET &server_sock, char* ip) {
	server_sock = createSocket();
    if (!validate(server_sock, -1, server_sock, "socket()")) {
		return -1;
	}

	sockaddr_in server_sockAddr;
	init_sockaddr(&server_sockAddr, ip);

	int ret;
	ret = bind(server_sock, (struct sockaddr*)&server_sockAddr, sizeof(server_sockAddr));
	if (ret == SOCKET_ERROR) {
		printError("bind() error:");
		closeSocket(server_sock);
		return -1;
	}
	printf("bind() succes\n");

	ret = listen(server_sock, 1);
	if (ret == SOCKET_ERROR) {
		printError("listen() error:");
		closeSocket(server_sock);
		return -1;
	}
	printf("listen() succes\n");

	int t = 1;
	setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(int));

	return 0;
}

SOCKET configureClient(char* ip) {
	SOCKET clientSock = createSocket();

	struct sockaddr_in clientSockAddr;
	init_sockaddr(&clientSockAddr, ip);

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

SOCKET configureUDP(Type type, char *ip, Context context) {
	SOCKET sock;
    struct timeval read_timeout;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (!validate(sock, INVALID_SOCKET, sock, "socket")) {
		return -1;
	}
    read_timeout.tv_sec = 1;
    read_timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
	if (type == SERVER) {
		sockaddr_in sockAddr;
		init_sockaddr(&sockAddr, ip);
        int ret = bind(sock, (struct sockaddr*)&sockAddr, sizeof(sockAddr));
		if (!validate(ret, SOCKET_ERROR, sock, "bind")) {
			return -1;
		}
	}
    else {
        init_sockaddr(context.sock_addr, ip);
    }

	return sock;
}

void tcpServer(SOCKET server_sock) {

	SOCKET connectedSock;
	struct sockaddr_in connectedSockAddr;
	socklen_t sockAddrLen = sizeof(struct sockaddr_in);
    Context context;
    context.sock_addr = (sockaddr_in*)malloc(sizeof(sockaddr_in));
    context.protocol = TCP;
    context.seq_number = (unsigned char*)malloc(sizeof(unsigned char*));
    *context.seq_number = 0;
    while (true) {
        connectedSock = accept(server_sock, (struct sockaddr*)&connectedSockAddr, &sockAddrLen);
        if (connectedSock == INVALID_SOCKET) {
            printError("accept() error:");
            closeSocket(server_sock);
            return;
        }
        printf("Client(%s) connected\n", inet_ntoa(connectedSockAddr.sin_addr));

        connectedSock = setupKeepalive(connectedSock);
        if (connectedSock == -1) {
            printError("keepalive() error:");
            closeSocket(server_sock);
            return;
        }
        while (serverListener(connectedSock, context) != -1) {}
        printf("Client(%s) disconnected\n", inet_ntoa(connectedSockAddr.sin_addr));
    }
}

void udpServer(SOCKET server_sock) {
    Context context;
    context.sock_addr = (sockaddr_in*)malloc(sizeof(sockaddr_in));
    context.seq_number = (unsigned char*)malloc(sizeof(unsigned char*));
    *context.seq_number = 0;
    context.protocol = UDP;
    while (true) {
        while (serverListener(server_sock, context) != -1){}
        *context.seq_number = 0;
    }
}

int serverListener(SOCKET sock, Context context) {
	int received;
    struct timeval read_timeout;
    char buffer[MESSAGE_MAX_SIZE];

    if (context.protocol == UDP) {
        read_timeout.tv_sec = 0;
        read_timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
    }

    received = _recv(sock, buffer, context);
    if (received == -1) {
        printError("Server failure : ");
        return -1;
    }

    if (context.protocol == UDP) {
        read_timeout.tv_sec = 1;
        read_timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
    }

    buffer[received] = '\0';

    if (!strncmp(buffer, "TIME", 4)) {
        time_server(sock, context);
        return 0;
    }
    else if (!strncmp(buffer, "ECHO", 4)) {
        echo_server(sock, context);
        return 0;
    }
    else if (!strncmp(buffer, "CLOSE", 5)) {
        printf("CLOSE command\n");
        return -1;
    }
    else if (!strcmp(buffer, "UPLOAD")) {
        if (upload_server(sock, context) == -1) {
            return -1;
        }
        return 0;
    }

    else if (!strcmp(buffer, "DOWNLOAD")) {
        if (download_server(sock, context) == -1) {
            return -1;
        }
        return 0;
    }
	return 0;
}

void clientListener(SOCKET sock, Context context) {
	bool exit = false;
	printHelp();

	while (!exit) {
		printf(">");
		char* command = (char*)malloc(COMMAND_LENGTH);
		fgets(command, COMMAND_LENGTH, stdin);
		command[strlen(command) - 1] = 0;

		if (checkCommand(command)) {
            if (_send(sock, command, (unsigned char)strlen(command), context) == -1 ) {
                printError("Client failed : ");
                continue;
            }

            if (!strcmp(command, "TIME")) {
                time_client(sock, context);
            }
            else if (!strncmp(command, "ECHO", 4)) {
                echo_client(sock, context);
            }
            else if (!strcmp(command, "CLOSE")) {
                exit = true;
            }
            else if (!strcmp(command, "UPLOAD")) {
                upload_client(sock, context);
            }
            else if (!strcmp(command, "DOWNLOAD")) {
                download_client(sock, context);
            }
		}
		else {
			printf("Wrong command\n");
		}
		free(command);
	}
}
