#include "gthr.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

// experiments.

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
	pollfd pfd;
	pfd.fd = connfd;
	pfd.events = POLLOUT;
	char *test = "HTTP/1.1 200 OK\r\nDate: now\r\nContent-Type: text/html\r\nConnection: Closed\r\n\r\n<meta charset=\"utf-8\"><title>yes</title>yes, that's right, this site is made in C with my gthr library. you are currently being served by a green thread especially created for YOU!<br><code>⠀⠀⠀⡯⡯⡾⠝⠘⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢊⠘⡮⣣⠪⠢⡑⡌<br>⠀⠀⠀⠟⠝⠈⠀⠀⠀⠡⠀⠠⢈⠠⢐⢠⢂⢔⣐⢄⡂⢔⠀⡁⢉⠸⢨⢑⠕⡌<br>⠀⠀⡀⠁⠀⠀⠀⡀⢂⠡⠈⡔⣕⢮⣳⢯⣿⣻⣟⣯⣯⢷⣫⣆⡂⠀⠀⢐⠑⡌<br>⢀⠠⠐⠈⠀⢀⢂⠢⡂⠕⡁⣝⢮⣳⢽⡽⣾⣻⣿⣯⡯⣟⣞⢾⢜⢆⠀⡀⠀⠪<br>⣬⠂⠀⠀⢀⢂⢪⠨⢂⠥⣺⡪⣗⢗⣽⢽⡯⣿⣽⣷⢿⡽⡾⡽⣝⢎⠀⠀⠀⢡<br>⣿⠀⠀⠀⢂⠢⢂⢥⢱⡹⣪⢞⡵⣻⡪⡯⡯⣟⡾⣿⣻⡽⣯⡻⣪⠧⠑⠀⠁⢐<br>⣿⠀⠀⠀⠢⢑⠠⠑⠕⡝⡎⡗⡝⡎⣞⢽⡹⣕⢯⢻⠹⡹⢚⠝⡷⡽⡨⠀⠀⢔<br>⣿⡯⠀⢈⠈⢄⠂⠂⠐⠀⠌⠠⢑⠱⡱⡱⡑⢔⠁⠀⡀⠐⠐⠐⡡⡹⣪⠀⠀⢘<br>⣿⣽⠀⡀⡊⠀⠐⠨⠈⡁⠂⢈⠠⡱⡽⣷⡑⠁⠠⠑⠀⢉⢇⣤⢘⣪⢽⠀⢌⢎<br>⣿⢾⠀⢌⠌⠀⡁⠢⠂⠐⡀⠀⢀⢳⢽⣽⡺⣨⢄⣑⢉⢃⢭⡲⣕⡭⣹⠠⢐⢗<br>⣿⡗⠀⠢⠡⡱⡸⣔⢵⢱⢸⠈⠀⡪⣳⣳⢹⢜⡵⣱⢱⡱⣳⡹⣵⣻⢔⢅⢬⡷<br>⣷⡇⡂⠡⡑⢕⢕⠕⡑⠡⢂⢊⢐⢕⡝⡮⡧⡳⣝⢴⡐⣁⠃⡫⡒⣕⢏⡮⣷⡟<br>⣷⣻⣅⠑⢌⠢⠁⢐⠠⠑⡐⠐⠌⡪⠮⡫⠪⡪⡪⣺⢸⠰⠡⠠⠐⢱⠨⡪⡪⡰<br>⣯⢷⣟⣇⡂⡂⡌⡀⠀⠁⡂⠅⠂⠀⡑⡄⢇⠇⢝⡨⡠⡁⢐⠠⢀⢪⡐⡜⡪⡊<br>⣿⢽⡾⢹⡄⠕⡅⢇⠂⠑⣴⡬⣬⣬⣆⢮⣦⣷⣵⣷⡗⢃⢮⠱⡸⢰⢱⢸⢨⢌<br>⣯⢯⣟⠸⣳⡅⠜⠔⡌⡐⠈⠻⠟⣿⢿⣿⣿⠿⡻⣃⠢⣱⡳⡱⡩⢢⠣⡃⠢⠁<br>⡯⣟⣞⡇⡿⣽⡪⡘⡰⠨⢐⢀⠢⢢⢄⢤⣰⠼⡾⢕⢕⡵⣝⠎⢌⢪⠪⡘⡌⠀<br>⡯⣳⠯⠚⢊⠡⡂⢂⠨⠊⠔⡑⠬⡸⣘⢬⢪⣪⡺⡼⣕⢯⢞⢕⢝⠎⢻⢼⣀⠀<br>⠁⡂⠔⡁⡢⠣⢀⠢⠀⠅⠱⡐⡱⡘⡔⡕⡕⣲⡹⣎⡮⡏⡑⢜⢼⡱⢩⣗⣯⣟<br>⢀⢂⢑⠀⡂⡃⠅⠊⢄⢑⠠⠑⢕⢕⢝⢮⢺⢕⢟⢮⢊⢢⢱⢄⠃⣇⣞⢞⣞⢾<br>⢀⠢⡑⡀⢂⢊⠠⠁⡂⡐⠀⠅⡈⠪⠪⠪⠣⠫⠑⡁⢔⠕⣜⣜⢦⡰⡎⡯⡾⡽</code>";
	if (!gthr_wait_pollfd(gt, pfd)) {
		write(connfd, test, strlen(test) + 1);
	};
	gthr_create(gt->gl, &also_read, connfd);
	// close(connfd);
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
	pollfd pfd;
	pfd.fd = sockfd;
	pfd.events = POLLIN;
	printf("- Listening\n");
	while (1) {
		listen(sockfd, 128);
		printf("- Accepting new connection... \n");
		gthr_wait_pollfd(gt, pfd);
		socklen_t client_len = sizeof(addr);
		int connfd = accept(sockfd, (struct sockaddr *)&addr, &client_len);
		printf("- Got connection! fd %d\n", connfd);
		if (connfd >= 0) gthr_create(gt->gl, &handle_conn, connfd);
	}

	close(sockfd);
}

void gmain(gthr *gt, char *v) {
	gthr_create(gt->gl, &accept_conn, NULL);
	while (1) {
		printf("-[ignore] probing test\n");
		gthr_delay(gt, 1500);
	}
}

int main() {
	gthr_loop gl;
	gthr_loop_init(&gl);
	gthr_create(&gl, &gmain, NULL);
	while (1) gthr_loop_run(&gl);
	return 0;
}