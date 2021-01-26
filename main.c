#include "gthr.h"

#include <stdio.h>

// experiments.

void ha(gthr* gt, char *data) {
	printf("%s\n", data);
	while (1) {
		printf("----\n1s\n");
		gthr_delay(gt, 1000);
	}
}

void ho(gthr* gt, char *data) {
	printf("%s\n", data);
	while (1) {
		printf("2s\n");
		gthr_delay(gt, 2000);
	}
}

void hi(gthr* gt, char *data) {
	printf("%s\n", data);
	int c = 0;
	while (1) {
		printf("4s\n");
		gthr_delay(gt, 4000);
	}
}

void gmain(gthr *gt, char *v) {
	printf("bruh\n");
	gthr_create(gt->gl, &ha, "i am gthread 1");
	gthr_create(gt->gl, &ho, "i am gthread 2");
	gthr_create(gt->gl, &hi, "i am gthread 3");
	printf("bruh\n");
}

int main() {
	gthr_loop gl;
	gthr_loop_init(&gl);
	gthr_create(&gl, &gmain, "NULL");
	while (1) gthr_loop_run(&gl);
	return 0;
}