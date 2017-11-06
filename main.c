#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include "workflow.h"

#define SLAVES_NUMBER 3

struct slave {
    pid_t pid;
    int socket;
    bool busy;
    struct sockaddr_in sock_addr;
};

SOCKET configure_server(char* ip);
SOCKET configureClient(char* ip);
int serverListener(SOCKET server_sock);
void clientListener(SOCKET sock);
SOCKET connect_tcp(SOCKET server_sock, struct sockaddr_in *connected_sock_addr);
int make_slaves(void);
void master_process(char* ip);
static void send_fd(int socket, int fd);
static int recv_fd(int socket);
void free_slaves(int sig, siginfo_t *siginfo, void *context);

int max_sd = 0;
struct slave slaves[SLAVES_NUMBER];
int free_slaves_numb = SLAVES_NUMBER;

int main(int argc, char* argv[]) {
    SOCKET clientSock;
    SOCKET server_sock;
	pid_t parent = getpid();
	struct sigaction act;
	memset (&act, '\0', sizeof(act));
	act.sa_sigaction = &free_slaves;
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGUSR1, &act, NULL) < 0) {
		perror ("sigaction");
	}
    if (!strcmp(argv[1], "server")) {
		int ret = make_slaves();
		if (ret == 0) {
			master_process(argv[2]);
			return 0;
		} else if (ret == -1) {
			exit(-1);
		}
		sleep(1);
		while (true) {
			SOCKET connected_sock = recv_fd(ret);
			while (serverListener(connected_sock) != -1) {}
			kill(parent, SIGUSR1);
			printf("Client disconnected\n");
		}
		closeSocket(server_sock);
    }
    else if (!strcmp(argv[1], "client")) {
		if ((clientSock = configureClient(argv[2])) != -1) {
			clientListener(clientSock);
		}
		closeSocket(clientSock);
    }
    return 0;
}


int make_slaves(void) {
    int pid;
    int sv[2];

    for (int i = 0; i < SLAVES_NUMBER; i++) {
		if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) != 0)
			printError("Failed to create Unix-domain socket pair: ");
        pid = fork();
        if (pid == -1) {
            printError("fork() : ");
            return -1;
        } else if (pid == 0) {
            return sv[1];
        }
		slaves[i].busy = false;
        slaves[i].pid = pid;
        slaves[i].socket = sv[0];
    }
    return 0;
}

static void send_fd(int socket, int fd)
{
    struct msghdr msg = { 0 };
    char buf[CMSG_SPACE(sizeof(fd))], dup[256];
    memset(buf, '\0', sizeof(buf));
    struct iovec io = { .iov_base = &dup, .iov_len = 3 };

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));

    *((int *) CMSG_DATA(cmsg)) = fd;

    msg.msg_controllen = cmsg->cmsg_len;

    if (sendmsg(socket, &msg, 0) < 0)
        printError("Failed to send message: ");
}

static int recv_fd(int socket)  // receive fd from socket
{
    struct msghdr msg = {0};

    char m_buffer[256];
    struct iovec io = { .iov_base = m_buffer, .iov_len = sizeof(m_buffer) };
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    char c_buffer[256];
    msg.msg_control = c_buffer;
    msg.msg_controllen = sizeof(c_buffer);

    if (recvmsg(socket, &msg, 0) < 0)
        printError("Failed to receive message: ");

    struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg);

    unsigned char * data = CMSG_DATA(cmsg);

    int fd = *((int*) data);

    return fd;
}


SOCKET configure_server(char* ip) {
	SOCKET server_sock;
	struct sockaddr_in server_sockAddr;
	server_sock = createSocket();
	if (!validate(server_sock, -1, server_sock, "socket()")) {
		return -1;
	}

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

	return server_sock;
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

void free_slaves(int sig, siginfo_t *siginfo, void *context) {
	int pos;
	if (sig == SIGUSR1) {
		pos = 0;
		while (slaves[pos].pid != siginfo->si_pid) {
			pos++;
		}
		slaves[pos].busy = false;
		free_slaves_numb++;
	}
}

void master_process(char *ip) {
	SOCKET connected_sock, socket;
    int free_pos;
	socket = configure_server(ip);
    while (true) {
        while (free_slaves_numb != 0) {
            free_pos = 0;
            while (slaves[free_pos].busy) {
                free_pos++;
            }
            connected_sock = connect_tcp(socket, &(slaves[free_pos].sock_addr));
			if (connected_sock == -1) {
				continue;
			}
            send_fd(slaves[free_pos].socket, connected_sock);
			slaves[free_pos].busy = true;
			free_slaves_numb--;
        }
		pause();
    }
}

SOCKET connect_tcp(SOCKET server_sock, struct sockaddr_in *connected_sock_addr) {

	SOCKET connected_sock;
	socklen_t sockAddrLen = sizeof(struct sockaddr_in);
    connected_sock = accept(server_sock, (struct sockaddr*)connected_sock_addr, &sockAddrLen);
    if (connected_sock == INVALID_SOCKET) {
        printError("accept() error:");
        closeSocket(server_sock);
        return -1;
    }
    printf("Client(%s) connected\n", inet_ntoa(connected_sock_addr->sin_addr));

    connected_sock = setupKeepalive(connected_sock);
    if (connected_sock == -1) {
        printError("keepalive() error:");
        closeSocket(server_sock);
        return -1;
    }
    return connected_sock;
}

int serverListener(SOCKET sock) {
	int received;
    char buffer[MESSAGE_MAX_SIZE];

    received = tcp_recv(sock, buffer);
    if (received == -1) {
        printError("Server failure : ");
        return -1;
    }

    buffer[received] = '\0';

    if (!strncmp(buffer, "TIME", 4)) {
        time_server(sock);
        return 0;
    }
    else if (!strncmp(buffer, "ECHO", 4)) {
        echo_server(sock);
        return 0;
    }
    else if (!strncmp(buffer, "CLOSE", 5)) {
        printf("CLOSE command\n");
        return -1;
    }
    else if (!strcmp(buffer, "UPLOAD")) {
        if (upload_server(sock) == -1) {
            return -1;
        }
        return 0;
    }

    else if (!strcmp(buffer, "DOWNLOAD")) {
        if (download_server(sock) == -1) {
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
            if (tcp_send(sock, command, (unsigned char)strlen(command)) == -1 ) {
                printError("Client failed : ");
                continue;
            }

            if (!strcmp(command, "TIME")) {
                time_client(sock);
            }
            else if (!strncmp(command, "ECHO", 4)) {
                echo_client(sock);
            }
            else if (!strcmp(command, "CLOSE")) {
                exit = true;
            }
            else if (!strcmp(command, "UPLOAD")) {
                upload_client(sock);
            }
            else if (!strcmp(command, "DOWNLOAD")) {
                download_client(sock);
            }
		}
		else {
			printf("Wrong command\n");
		}
		free(command);
	}
}
