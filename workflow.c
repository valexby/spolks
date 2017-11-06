#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include "workflow.h"

const char OK_MSG[] = "ok";
const char END_MSG[] = "end";

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

void init_sockaddr(struct sockaddr_in *sock, const char* ip) {
	(*sock).sin_family = AF_INET;
	(*sock).sin_addr.s_addr = inet_addr(ip);
    (*sock).sin_port = htons(PORT);
}

int time_server(SOCKET socket) {
    char* time = getCurrentTime();
    printf("TIME command\n");
    if (tcp_send(socket, time, (char)strlen(time)) == -1) {
        printError("TIME failed : ");
        return -1;
    }
    return 0;
}

int time_client(SOCKET socket) {
    int received;
    char buffer[MESSAGE_MAX_SIZE];
    received = tcp_recv(socket, buffer);
    if (received == -1) {
        printError("TIME failed : ");
        return -1;
    }
    buffer[received] = '\0';
    printf("Server time:%s", buffer);
    return 0;
}

int echo_server(SOCKET socket) {
    char buffer[MESSAGE_MAX_SIZE];
    printf("ECHO command\n");
    if (tcp_send(socket, buffer + 5, strlen(buffer) - 5) == -1) {
        printError("Echo failed : ");
        return -1;
    }
    return 0;
}

int echo_client(SOCKET socket) {
    char buffer[MESSAGE_MAX_SIZE];
    int received = tcp_recv(socket, buffer);
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

int upload_client(SOCKET socket) {
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
        if (tcp_send(socket, buffer, readed) == -1) {
            printError("Upload aborted : ");
            fclose(file);
            return -1;
        }
        printf("[%1.00f/100]\r", (float)(((float)total / (float)size) * 100));
    }
    printf("\n");
    if (tcp_send(socket, END_MSG, 3) == -1) {
        printError("Upload aborted : ");
        fclose(file);
        return -1;
    }
    time(&second_tape);
    printf("Speed : %.2lfMB/s\n", ((double)size) / ((second_tape - first_tape) * 1024 * 1024) );
    fclose(file);
    return 0;
}

int upload_server(SOCKET socket) {
	FILE *file;
	int received;
    char buffer[MESSAGE_MAX_SIZE];
    file = fopen(FILE_PATH_DOWNLOAD, "wb");

    printf("UPLOAD command\n");
    received = tcp_recv(socket, buffer);
    while (strncmp(buffer, END_MSG, 3))
    {
        if (received < 0) {
            printError("Upload aborted : ");
            fclose(file);
            return -1;
        }
        fwrite(buffer, 1, received, file);
        received = tcp_recv(socket, buffer);
    }
    fclose(file);
    return 0;
}

int download_server(SOCKET socket)
{
	FILE *file;
	int readed = 0;
    char buffer[MESSAGE_MAX_SIZE];
    printf("DOWNLOAD command\n");

    file = fopen(FILE_PATH_UPLOAD, "r+b");
    fseek(file, 0, SEEK_END);
    sprintf(buffer, "%ld", ftell(file));
    if (tcp_send(socket, buffer, (unsigned char)strlen(buffer)) == -1) {
        printError("Download aborted : ");
        fclose(file);
        return -1;
    }
    fseek(file, 0, SEEK_SET);

    while ((readed = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        if (tcp_send(socket, buffer, readed) <= 0) {
            printError("Download aborted : ");
            fclose(file);
            return -1;
        }
    }
    if (tcp_send(socket, END_MSG, 3) == -1) {
        printError("Download aborted : ");
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;

}

int download_client(SOCKET socket)
{
	FILE *file;
	int received, readed;
    size_t size;
    time_t first_tape, second_tape;
    char buffer[MESSAGE_MAX_SIZE];
    file = fopen(FILE_PATH_DOWNLOAD, "wb");

    if (tcp_recv(socket, buffer) == -1) {
        printError("Download aborted : ");
        fclose(file);
        return -1;
    }
    size = atol(buffer);
    time(&first_tape);
    received = tcp_recv(socket, buffer);
    while (strncmp(buffer, "end", 3)) {
        if (received == -1) {
            printError("Download aborted : ");
            fclose(file);
            return -1;
        }
        readed += received;
        fwrite(buffer, 1, received, file);
        received = tcp_recv(socket, buffer);
        printf("[%1.00f/100]\r", (float)(((float)readed / (float)size) * 100));
    }
    time(&second_tape);
    printf("Speed : %.2lfMB/s", ((double)size) / ((second_tape - first_tape) * 1024 * 1024) );
    printf("\n");
    fclose(file);
    return 0;
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
