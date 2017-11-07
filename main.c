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
#include <fcntl.h>
#include "workflow.h"

#define SLAVES_NUMBER 3

struct slave {
    pid_t pid;
    int socket;
    bool busy;
    struct sockaddr_in sock_addr;
};

SOCKET configure_server(char* ip);
SOCKET configure_client(char* ip);
int server_listener(SOCKET server_sock);
void client_listener(SOCKET sock);
SOCKET connect_tcp(SOCKET server_sock, struct sockaddr_in *connected_sock_addr);
int make_slaves(void);
void master_process(char* ip);
static void send_fd(int socket, int fd);
static int recv_fd(int socket);
void free_slaves(int sig, siginfo_t *siginfo, void *context);
void sig_handler(int signo);

int max_sd = 0;
struct slave slaves[SLAVES_NUMBER];
int free_slaves_numb = SLAVES_NUMBER;
bool interrupted = false;

int main(int argc, char* argv[]) {
    SOCKET client_sock;
    SOCKET server_sock;
	pid_t parent = getpid();
	unsigned char flag = 1;
	struct sigaction act;
	memset (&act, '\0', sizeof(act));
	act.sa_sigaction = &free_slaves;
	act.sa_flags = SA_SIGINFO;
	signal(SIGURG, sig_handler);
	signal(SIGUSR2, sig_handler);
	signal(SIGPIPE, sig_handler);
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
			fcntl(connected_sock, F_SETOWN, getpid());
			while (server_listener(connected_sock) != -1) {}
			kill(parent, SIGUSR1);
			if (interrupted) {
				interrupted = false;
			} else {
				send(connected_sock, &flag, 1, MSG_OOB);
			}
			shutdown(connected_sock, SHUT_RDWR);
			close(connected_sock);
		}
		close(server_sock);
    }
    else if (!strcmp(argv[1], "client")) {
		if ((client_sock = configure_client(argv[2])) != -1) {
			client_listener(client_sock);
		}
		if (interrupted) {
			interrupted = false;
		} else {
			send(client_sock, &flag, 1, MSG_OOB);
		}
		shutdown(client_sock, SHUT_RDWR);
		close(client_sock);
    }
    return 0;
}

void sig_handler(int signo) {
	printf("Connection was closed\n");
	interrupted = true;
}

int make_slaves(void) {
    int pid;
    int sv[2];

    for (int i = 0; i < SLAVES_NUMBER; i++) {
		if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) != 0)
			print_error("Failed to create Unix-domain socket pair: ");
        pid = fork();
        if (pid == -1) {
            print_error("fork() : ");
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
        print_error("Failed to send message: ");
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
        print_error("Failed to receive message: ");

    struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg);

    unsigned char * data = CMSG_DATA(cmsg);

    int fd = *((int*) data);

    return fd;
}


SOCKET configure_server(char* ip) {
	SOCKET server_sock;
	struct sockaddr_in server_sockAddr;
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock == -1) {
		perror("Cannot create socket : ");
		close(server_sock);
		return -1;
	}

	init_sockaddr(&server_sockAddr, ip);

	int ret;
	ret = bind(server_sock, (struct sockaddr*)&server_sockAddr, sizeof(server_sockAddr));
	if (ret == SOCKET_ERROR) {
		print_error("bind() error:");
		if (errno == 98) exit(1);
		close(server_sock);
		return -1;
	}
	printf("bind() succes\n");

	ret = listen(server_sock, 1);
	if (ret == SOCKET_ERROR) {
		print_error("listen() error:");
		close(server_sock);
		return -1;
	}
	printf("listen() succes\n");

	int t = 1;
	setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(int));

	return server_sock;
}

SOCKET configure_client(char* ip) {
	SOCKET client_sock = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in client_sock_addr;
	init_sockaddr(&client_sock_addr, ip);

	printf("Connection to the server\n");
	int ret = connect(client_sock, (struct sockaddr*)&client_sock_addr, sizeof(client_sock_addr));
	if (ret == SOCKET_ERROR) {
		print_error("connect() error:");
		return -1;
	}
	printf("Connected\n");
	client_sock = setup_socket(client_sock);
	if (client_sock == -1) {
		print_error("keepalive() error:");
		return -1;
	}
	fcntl(client_sock, F_SETOWN, getpid());

	return client_sock;
}

void free_slaves(int sig, siginfo_t *siginfo, void *context) {
	int pos;
	if (sig == SIGUSR1) {
		pos = 0;
		while (slaves[pos].pid != siginfo->si_pid) {
			pos++;
		}
		slaves[pos].busy = false;
		printf("Client(%s) disconected\n", inet_ntoa(slaves[pos].sock_addr.sin_addr));
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
        print_error("accept() error:");
        return -1;
    }
    printf("Client(%s) connected\n", inet_ntoa(connected_sock_addr->sin_addr));

    connected_sock = setup_socket(connected_sock);
    if (connected_sock == -1) {
        print_error("keepalive() error:");
        close(server_sock);
        return -1;
    }
    return connected_sock;
}

int server_listener(SOCKET sock) {
	int received, ret;
    char buffer[MESSAGE_MAX_SIZE], *in, *out;

    received = tcp_recv(sock, buffer);
    if (received == -1) {
        print_error("Server failure : ");
        return -1;
    }

    buffer[received] = '\0';

    if (!strncmp(buffer, "TIME", 4)) {
        ret = time_server(sock);
    }
    else if (!strncmp(buffer, "ECHO", 4)) {
        ret = echo_server(sock, buffer);
    }
    else if (!strncmp(buffer, "CLOSE", 5)) {
        printf("CLOSE command\n");
        return -1;
    }
    else if (!strncmp(buffer, "UPLOAD", 6)) {
		strtok(buffer, " ");
		in = strtok(NULL, " ");
		out = strtok(NULL, " ");
        ret = upload_server(sock, out) == -1;
    }

    else if (!strncmp(buffer, "DOWNLOAD", 8)) {
		strtok(buffer, " ");
		in = strtok(NULL, " ");
		out = strtok(NULL, " ");
		ret = download_server(sock, in);
    }
	if (ret == -1) {
		printf("Aborted\n");
		return -1;
	}
	return 0;
}

void client_listener(SOCKET sock) {
	char *in, *out;
	char command[COMMAND_LENGTH];
	int ret;
	printHelp();

	while (strncmp(command, "CLOSE", 5)) {
		printf(">");
		fgets(command, COMMAND_LENGTH, stdin);
		command[strlen(command) - 1] = 0;
		if (!check_command(command)) {
			printf("Wrong command\n");
			continue;
		}
		if (tcp_send(sock, command, strlen(command)) == -1 ) {
			print_error("Client failed : ");
			continue;
		}

		if (!strcmp(command, "TIME")) {
			ret = time_client(sock);
		}
		else if (!strncmp(command, "ECHO", 4)) {
			ret = echo_client(sock);
		}
		else if (!strncmp(command, "UPLOAD", 6)) {
			strtok(command, " ");
			in = strtok(NULL, " ");
			out = strtok(NULL, " ");
			ret = upload_client(sock, in);
		}
		else if (!strncmp(command, "DOWNLOAD", 8)) {
			strtok(command, " ");
			in = strtok(NULL, " ");
			out = strtok(NULL, " ");
			ret = download_client(sock, out);
		}
		if (ret == -1) {
			printf("Aborted\n");
			break;
		}
	}
}
