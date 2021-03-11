#ifndef GTHR_H
#define GTHR_H

#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <poll.h>
#include <sys/socket.h>

enum yield_status {
	GTHR_RETURN,
	GTHR_YIELD,
	GTHR_LAISSEZ
};

struct gthr {
	char 				*sdata;
	ucontext_t 			ucp;
	enum yield_status	ystat;
	short				werr;
	struct timespec		time;
	struct gthr_loop 	*gl;
	void				(*fun)(void*);
	void				*args;
	struct gthr			*next, *prev;
};

struct gthr_loop {
	ucontext_t		ucp;
	struct pollfd	*pfd;
	size_t			pfdl, pfdc;
	struct gthr		**inpoll;
	struct gthr		**sleep;
	size_t			sleepl, sleepc;

	struct gthr		*head, *tail;
	int				minto;
};

extern _Thread_local struct gthr_loop	*_gthr_loop;
extern _Thread_local struct gthr		*_gthr;

void gthr_create(void (*fun)(void*), void *args);
void gthr_create_on(struct gthr_loop *gl, void (*fun)(void*), void *args);
int gthr_init(struct gthr *gt, size_t size);
void gthr_destroy(struct gthr *gt);

void gthr_loop_wrap(struct gthr *gt);

void gthr_yield(void);

int gthr_wait_pollfd(struct pollfd pfd);
int gthr_wait_readable(int fd);
int gthr_wait_writeable(int fd); 

ssize_t gthr_read(int fd, void *buf, size_t count);
ssize_t gthr_recv(int sockfd, void *buf, size_t length, int flags);
ssize_t gthr_write(int fd, void *buf, size_t count);
ssize_t gthr_send(int sockfd, void *buf, size_t length, int flags);

void gthr_delay(long ms);

void gthr_loop_init(struct gthr_loop *gl);

void gthr_loop_do(void);
int gthr_loop_wakeup(void);
void gthr_loop_poll(int timeout);

void gthr_loop_run(struct gthr_loop *gl);

#endif
