#include "gthr.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>

// experiments.

void handle_conn(gthr* gt, int connfd) {
	printf("handling client with connfd %d\n", connfd);
	pollfd pfd;
	pfd.fd = connfd;
	pfd.events = POLLOUT;
	while (1) {
		gthr_wait_pollfd(gt, pfd);
		write(connfd, "Hello !\r\n", 10);
		printf("sent hello\n");
		gthr_delay(gt, 1000);
	} 
	close(connfd);
}

void accept_conn(gthr* gt, void* _) {
	struct sockaddr_in addr;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	printf("%d\n", sockfd);

	int opt_val = 1;
	int m = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt_val, sizeof(opt_val));
	if (m < 0) {
		printf("m failure\n");
	}
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(4269);
	if (bind(sockfd, &addr, sizeof(addr)) < 0) {
		printf("bind failure\n");
	}
	pollfd pfd;
	pfd.fd = sockfd;
	pfd.events = POLLIN;
	printf("Listening\n");
	while (1) {
		int con = listen(sockfd, 128);
		if (con < 0) {
			printf("con failed\n");
			continue;
		}
		printf("1\n");
		gthr_wait_pollfd(gt, pfd);
		printf("2\n");
		socklen_t client_len = sizeof(addr);
		int connfd = accept(sockfd, (struct sockaddr *)&addr, &client_len);
		printf("- Got connection! fd %d\n", connfd);
		printf("errno is %d\n", errno);
		if (connfd >= 0) gthr_create(gt->gl, &handle_conn, connfd);
	}

	close(sockfd);
}

void gmain(gthr *gt, char *v) {
	gthr_create(gt->gl, &accept_conn, NULL);
	while (1) {
		printf("next\n");
		gthr_delay(gt, 800);
	}
}

int main() {
	gthr_loop gl;
	gthr_loop_init(&gl);
	gthr_create(&gl, &gmain, NULL);
	while (1) gthr_loop_run(&gl);
	return 0;
}