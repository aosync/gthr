#include "gthr.h"


// #include "ext/gthr_curl.h"
//#include "gthr_net.h"
//#include "gthr_ssl_net.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

// experiments.
/*
void also_read(gthr* gt, int connfd) {
	pollfd pfd;
	pfd.fd = connfd;
	pfd.events = POLLIN;
	char buf[40];
	int r;
	while (1) {
		if (!gthr_wait_pollfd(gt, pfd)) {
			r = read(connfd, buf, 40);
			printf("%s", buf);
			if (r != 40) break;
		} else {
			break;
		};	
	}
	printf("\n\nfinished printing\n");
	close(connfd);
}

void handle_conn(gthr* gt, int connfd) {
	printf("handling client with connfd %d\n", connfd);
	char *test = "HTTP/1.1 200 OK\r\nDate: now\r\nContent-Type: text/html\r\nConnection: Closed\r\nContent-Length: 290\r\n\r\n<meta charset=\"utf-8\"><title>yes</title>yes, that's right, this site is made in C with my gthr library. you are currently being served by a green thread especially created for YOU!";
	gthr_write(gt, connfd, test, strlen(test) + 1);
	char buf[500];
	gthr_read(gt, connfd, buf, 500);
	printf("%s\n", buf);
	// gthr_create(gt->gl, &also_read, connfd);
	close(connfd);
}

void accept_conn(gthr* gt, void* _) {
	struct sockaddr_in addr;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	int opt_val = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt_val, sizeof(opt_val));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(4269);
	bind(sockfd, &addr, sizeof(addr));
	printf("- Listening\n");
	while (1) {
		listen(sockfd, 128);
		printf("- Accepting new connection... \n");
		gthr_wait_readable(gt, sockfd);
		socklen_t client_len = sizeof(addr);
		int connfd = accept(sockfd, (struct sockaddr *)&addr, &client_len);
		printf("- Got connection! fd %d\n", connfd);
		if (connfd >= 0) gthr_create(gt->gl, &handle_conn, connfd);
	}

	close(sockfd);
}

void gmain(gthr *gt, char *v) {
	char *txt = "GET / HTTP/1.1\r\n\r\n";
	int sockfd = gthr_tcpdial(gt, "127.0.0.1", 631);
	printf("sockfd is %d\n", sockfd);
	int i = gthr_write(gt, sockfd, txt, strlen(txt));
	gthr_yield(gt);
	char buf[16];
	while (1) {
		memset(buf, 0, 16);
		int rc = gthr_read(gt, sockfd, buf, 15);
		buf[15] = '\0';
		printf("%s", buf);
		if (rc < 15) break;
	}
	printf("closed\n");
	close(sockfd);
}*/

int i = 0;

void gm(void *v) {
	/*CURL *h = curl_easy_init();
	curl_easy_setopt(h, CURLOPT_URL, "https://aosync.me");
	curl_easy_setopt(h, CURLOPT_HEADERDATA, stdout);
	curl_easy_setopt(h, CURLOPT_NOBODY, 1);
	gthr_curl_perform(h); */
	//curl_easy_perform(h);
	/* curl_easy_cleanup(h); */
	i++;
	printf("%d finished\n", i);
}

void test(void *v) {
	int b = 0;
	while (b++ < 500000) {
		gthr_create(&gm, NULL);
		printf("%p\n", _gthr);
		gthr_yield();
	}
}

#include <stdio.h>

int main() {
	struct gthr_loop gl;
	gthr_loop_init(&gl);
	int a = 0;
	gthr_create_on(&gl, &test, NULL);
	gthr_create_on(&gl, &test, NULL);
	while (gthr_loop_run(&gl));
	printf("Deadlock!\n");

	gthr_loop_finish(&gl);
	return 0;
}
