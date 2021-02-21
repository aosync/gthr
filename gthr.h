#ifndef GTHR_H
#define GTHR_H

#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <poll.h>
#include <sys/socket.h>

typedef struct pollfd pollfd;
typedef struct gthr_loop gthr_loop;
typedef struct gthr gthr;

enum yield_status {
	GTHR_RETURN,
	GTHR_YIELD,
	GTHR_LAISSEZ
};

struct gthr {
	char 				*sdata;
	ucontext_t 			ucp;
	enum yield_status	snum;
	short				wnum;
	struct timespec		time;
	struct gthr_loop 	*gl;
	void				(*fun)(void*, void*);
	void				*args;
};

typedef gthr* gthrp;

#define T gthrp
#define gthrpll_equals(a, b) (0)
#include "ll.h"

#define T gthrp
#include "vec.h"

#define T pollfd
#include "vec.h"

struct gthr_loop {
	ucontext_t 	ucp;
	pollfdv		pfds;
	gthrpv		gts;
	gthrpv		sleep;
	gthrpll		eq;
	int			minto;
};

void gthr_create(gthr_loop *gl, void (*fun)(void*, void*), void *args);
int gthr_init(gthr *gt, size_t size);
void gthr_destroy(gthr *gt);

void gthr_loop_wrap(gthr *gt);

void gthr_yield(gthr *gt);

int gthr_wait_pollfd(gthr *gt, pollfd pfd);
int gthr_wait_readable(gthr *gt, int fd);
int gthr_wait_writeable(gthr *gt, int fd); 

ssize_t gthr_read(gthr *gt, int fd, void *buf, size_t count);
ssize_t gthr_recv(gthr *gt, int sockfd, void *buf, size_t length, int flags);
ssize_t gthr_write(gthr *gt, int fd, void *buf, size_t count);
ssize_t gthr_send(gthr *gt, int sockfd, void *buf, size_t length, int flags);

void gthr_delay(gthr *gt, long ms);

void gthr_loop_init(gthr_loop *gl);

void gthr_loop_do(gthr_loop *gl);
int gthr_loop_wakeup(gthr_loop *gl);
void gthr_loop_poll(gthr_loop *gl, int timeout);

void gthr_loop_run(gthr_loop *gl);

#endif
