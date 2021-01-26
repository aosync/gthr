#include "gthr.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/timerfd.h>

// experiments.

void wait(gthr* gt, int *am) {
	struct itimerspec timeout;
	int fd = timerfd_create(CLOCK_MONOTONIC, 0);
	timeout.it_value.tv_sec = *am;
	timeout.it_value.tv_nsec = 0;
	timeout.it_interval.tv_sec = 0; /* not recurring */
    timeout.it_interval.tv_nsec = 0;
	timerfd_settime(fd, 0, &timeout, NULL);
	pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;
	gthr_wait_pollfd(gt, pfd);
	close(fd);
}

void ha(gthr* gt, char *data) {
	printf("%s\n", data);
	int to = 2;
	while (1) {
		printf("async delay\n");
		wait(gt, &to);
	}
}

void ho(gthr* gt, char *data) {
	printf("%s\n", data);
	int to = 1;
	while (1) {
		printf("next\n");
		wait(gt, &to);
	}
}

int main() {
	gthr_loop gl;
	gthr_loop_init(&gl);
	gthr_create(&gl, &ha, "this is my data");
	gthr_create(&gl, &ho, "mhm");
	while (1) gthr_loop_run(&gl);
	return 0;
}