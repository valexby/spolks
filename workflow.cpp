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
	int optval = 1;
	socklen_t  optlen = sizeof(optval);

	if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) == SOCKET_ERROR) {
		return -1;
	}
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
	close(nooby);
}

void printError(const char error_message[]) {
	printf("%s", error_message);
	printf("%s\n", strerror(errno));
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

void init_sockaddr(sockaddr_in *sock, const char* ip) {
	(*sock).sin_family = AF_INET;
	(*sock).sin_addr.s_addr = inet_addr(ip);
    (*sock).sin_port = htons(PORT);
}

int time_server(SOCKET socket, Context context) {
    char* time = getCurrentTime();
    printf("TIME command\n");
    if (_send(socket, time, (char)strlen(time), context) == -1) {
        printError("TIME failed : ");
        return -1;
    }
    return 0;
}

int time_client(SOCKET socket, Context context) {
    int received;
    char buffer[MESSAGE_MAX_SIZE];
    received = _recv(socket, buffer, context);
    if (received == -1) {
        printError("TIME failed : ");
        return -1;
    }
    buffer[received] = '\0';
    printf("Server time:%s", buffer);
    return 0;
}

int echo_server(SOCKET socket, Context context) {
    char buffer[MESSAGE_MAX_SIZE];
    printf("ECHO command\n");
    if (_send(socket, buffer + 5, strlen(buffer) - 5, context) == -1) {
        printError("Echo failed : ");
        return -1;
    }
    return 0;
}

int echo_client(SOCKET socket, Context context) {
    char buffer[MESSAGE_MAX_SIZE];
    int received = _recv(socket, buffer, context);
    if (received == -1) {
        printError("ECHO failed : ");
        return -1;
    }
    buffer[received] = '\0';
    printf("ECHO result:%s\n", buffer);
    for (int i = 0; i < MESSAGE_MAX_SIZE; i++) {
        buffer[i] = '\0';
    }
    return 0;
}

int upload_client(SOCKET socket, Context context) {
	FILE *file;
	int total = 0, readed = 0;
    size_t size;
    time_t first_tape, second_tape;
    char buffer[MESSAGE_MAX_SIZE];

    file = fopen(FILE_PATH_UPLOAD, "r+b");
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    time(&first_tape);
    while ((readed = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        total += readed;
        if (_send(socket, buffer, readed, context) == -1) {
            printError("Upload aborted : ");
            fclose(file);
            return -1;
        }
        printf("[%1.00f/100]\r", (float)(((float)total / (float)size) * 100));
    }
    printf("\n");
    if (_send(socket, END_MSG, 3, context) == -1) {
        printError("Upload aborted : ");
        fclose(file);
        return -1;
    }
    time(&second_tape);
    printf("Speed : %.2lfMB/s\n", ((double)size) / ((second_tape - first_tape) * 1024 * 1024) );
    fclose(file);
    return 0;
}

int upload_server(SOCKET socket, Context context) {
	FILE *file;
	int received;
    char buffer[MESSAGE_MAX_SIZE];
    file = fopen(FILE_PATH_DOWNLOAD, "wb");

    printf("UPLOAD command\n");
    received = _recv(socket, buffer, context);
    while (strncmp(buffer, END_MSG, 3))
    {
        if (received < 0) {
            printError("Upload aborted : ");
            fclose(file);
            return -1;
        }
        fwrite(buffer, 1, received, file);
        received = _recv(socket, buffer, context);
    }
    fclose(file);
    return 0;
}

int download_server(SOCKET socket, Context context)
{
	FILE *file;
	int readed = 0;
    char buffer[MESSAGE_MAX_SIZE];
    printf("DOWNLOAD command\n");

    file = fopen(FILE_PATH_UPLOAD, "r+b");
    fseek(file, 0, SEEK_END);
    sprintf(buffer, "%ld", ftell(file));
    if (_send(socket, buffer, (unsigned char)strlen(buffer), context) == -1) {
        printError("Download aborted : ");
        fclose(file);
        return -1;
    }
    fseek(file, 0, SEEK_SET);

    while ((readed = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        if (_send(socket, buffer, readed, context) <= 0) {
            printError("Download aborted : ");
            fclose(file);
            return -1;
        }
    }
    if (_send(socket, END_MSG, 3, context) == -1) {
        printError("Download aborted : ");
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;

}

int download_client(SOCKET socket, Context context)
{
	FILE *file;
	int received, readed;
    size_t size;
    time_t first_tape, second_tape;
    char buffer[MESSAGE_MAX_SIZE];
    file = fopen(FILE_PATH_DOWNLOAD, "wb");

    if (_recv(socket, buffer, context) == -1) {
        printError("Download aborted : ");
        fclose(file);
        return -1;
    }
    size = atol(buffer);
    time(&first_tape);
    received = _recv(socket, buffer, context);
    while (strncmp(buffer, "end", 3)) {
        if (received == -1) {
            printError("Download aborted : ");
            fclose(file);
            return -1;
        }
        readed += received;
        fwrite(buffer, 1, received, file);
        received = _recv(socket, buffer, context);
        printf("[%1.00f/100]\r", (float)(((float)readed / (float)size) * 100));
    }
    time(&second_tape);
    printf("Speed : %.2lfMB/s", ((double)size) / ((second_tape - first_tape) * 1024 * 1024) );
    printf("\n");
    fclose(file);
    return 0;
}

int _send(SOCKET sock, const char* buf, size_t len, Context context) {
    switch (context.protocol) {
    case TCP:
        return tcp_send(sock, buf, len);
    case UDP:
        return udp_send(sock, buf, len, context);
    }
    return -1;
}

int tcp_send(SOCKET sock, const char* buf, size_t len) {
    int sent, received = -1;
    char ok[2];
    sent = send(sock, &len, sizeof(len), 0);
    if (sent != sizeof(len)) {
        return -1;
    }
    if (LOGGING) {
        printf("%d pytes sent\n", sent);
    }
    sent = send(sock, buf, len, 0);
    if (sent != (ssize_t)len) {
        return -1;
    }
    if (LOGGING) {
        printf("%d bytes sent\n", sent);
    }
    received = recv(sock, ok, 2, 0);
    if (LOGGING) {
        printf("%d bytes received\n", received);
    }
    if (strncmp(ok, OK_MSG, 2)) {
        return -1;
    }
	return sent;
}

int udp_send(SOCKET sock, const char* buf, size_t len, Context context) {
    socklen_t slen = sizeof(sockaddr_in);
    int sent, received = -1;
    char ok[2];
    while (received == -1)
    {
        sent = sendto(sock, context.seq_number, 1, 0, (struct sockaddr*) context.sock_addr, slen);
        if (sent == -1) {
            printError("Fucking send: ");
        }
        if (LOGGING) {
            printf("%d bytes sent\n", sent);
        }
        sent = sendto(sock, &len, sizeof(len), 0, (struct sockaddr*) context.sock_addr, slen);
        if (LOGGING) {
            printf("%d bytes sent\n", sent);
        }
        sent = sendto(sock, buf, len, 0, (struct sockaddr*) context.sock_addr, slen);
        if (LOGGING) {
            printf("%d bytes sent\n", sent);
        }
        received = recvfrom(sock, ok, 2, 0, (struct sockaddr*) context.sock_addr, &slen);
        if (LOGGING) {
            printf("%d bytes received\n", received);
        }
    }
    inc_seq_numb(context.seq_number);
	return sent;
}

int _recv(SOCKET sock, char* buf, Context context) {
    switch (context.protocol) {
    case TCP:
        return tcp_recv(sock, buf);
    case UDP:
        return udp_recv(sock, buf, context);
    }
    return -1;
}

int tcp_recv(SOCKET sock, char* buf) {
	int received = -1, sent;
    size_t len = 0;
    while (recv(sock, &len, sizeof(len), 0) == -1) {};
    received = recv(sock, buf, len, 0);
    if (received != (ssize_t)len) {
        return -1;
    }
    if (LOGGING) {
        printf("%d bytes received\n", received);
    }
    sent = send(sock, OK_MSG, 2, 0);
    if (LOGGING) {
        printf("%d bytes sent\n", sent);
    }
	return received;
}

int udp_recv(SOCKET sock, char* buf, Context context) {
	int received = -1, sent;
    size_t len = 0;
    socklen_t slen;
    unsigned char recv_seq;
    while (received == -1)
    {
        received = recvfrom(sock, &recv_seq, 1, 0, (struct sockaddr*) context.sock_addr, &slen);
        if (received == -1) continue;
        if (LOGGING) {
            printf("%d bytes received\n", received);
        }
        received = recvfrom(sock, &len, sizeof(len), 0, (struct sockaddr*) context.sock_addr, &slen);
        if (received == -1) continue;
        if (LOGGING) {
            printf("%d bytes received\n", received);
        }
        received = recvfrom(sock, buf, len, 0, (struct sockaddr*) context.sock_addr, &slen);
    }
    if (LOGGING) {
        printf("%d bytes received\n", received);
    }
    switch (comp_seq_numb(recv_seq, *context.seq_number)) {
    case 0:
        inc_seq_numb(context.seq_number);
        break;
    case 1:
        received = 0;
        break;
    case -1:
        printf("Somethin bad happend with seq numbers!\nCurrent %d\nReceived %d\n", *context.seq_number, recv_seq);
        return -1;
    }
    sent = sendto(sock, OK_MSG, 2, 0, (struct sockaddr*) context.sock_addr, slen);
    if (LOGGING) {
        printf("%d bytes sent\n", sent);
    }
	return received;
}

void inc_seq_numb(unsigned char *cur_seq_number) {
    if (*cur_seq_number == MAX_SEQ_NUMB) {
        *cur_seq_number = 0;
    }
    else {
        (*cur_seq_number)++;
    }
}

int comp_seq_numb(unsigned char first, unsigned char second) {
    if (first == second) {
        return 0;
    }
    if (first == 0 && second == MAX_SEQ_NUMB) {
        return -1;
    }
    if (first == MAX_SEQ_NUMB && second == 0) {
        return 1;
    }
    if (first > second) {
        return -1;
    }
    return 1;
}
