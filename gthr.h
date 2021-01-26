#include <stdlib.h>
#include <ucontext.h>
#include <stdio.h>
#include <time.h>
#include <poll.h>

typedef struct pollfd pollfd;
typedef struct gthr_loop gthr_loop;
typedef struct gthr gthr;

struct gthr {
	char 				*sdata;
	ucontext_t 			ucp;
	short				snum;
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

int gthr_init(gthr *gt, size_t size) {
	gt->gl = NULL;
	gt->args = NULL;
	getcontext(&gt->ucp);
	gt->sdata = malloc(size * sizeof(char));
	if (!gt->sdata)
		return 0;
	gt->ucp.uc_stack.ss_sp = gt->sdata;
	gt->ucp.uc_stack.ss_size = size * sizeof(char);
	return 1;
}

void gthr_free(gthr *gt) {
	free(gt->sdata);
	// free gt args too ? no! it's the gthread's role
}

void gthr_yield(gthr *gt) {
	gt->snum = -1;
	swapcontext(&gt->ucp, &gt->gl->ucp);
}

void gthr_wait_pollfd(gthr *gt, pollfd pfd) {
	pollfdv_push(&gt->gl->pfds, pfd);
	gthrpv_push(&gt->gl->gts, gt);
	gt->snum = 1;
	swapcontext(&gt->ucp, &gt->gl->ucp);
}

void gthr_delay(gthr *gt, long ms) {
	clock_gettime(CLOCK_MONOTONIC, &gt->time);
	gt->time.tv_sec += ms / 1000;
	gt->time.tv_nsec += (ms * 1000000) % 1000000000;
	gthrpv_push(&gt->gl->sleep, gt);
	gt->snum = 1;
	swapcontext(&gt->ucp, &gt->gl->ucp);
}

void gthr_loop_init(gthr_loop *gl) {
	gl->minto = 2000;
	gthrpv_init(&gl->gts);
	gthrpv_init(&gl->sleep);
	pollfdv_init(&gl->pfds);
	gthrpll_init(&gl->eq);
}

void gthr_wrap(gthr *gt) {
	gt->fun(gt, gt->args);
}

void gthr_loop_next(gthr_loop *gl) {
	if (gl->eq.head) {
		gthrpll_ *tmp = gl->eq.head;
		gl->eq.head = tmp->next;
		if (!tmp->next) gl->eq.back = tmp->next;

		gthr *gt = tmp->val;

		gt->snum = 0;
		swapcontext(&gl->ucp, &gt->ucp);

		switch(gt->snum) {
		case -1:
			// gthread yielded. append to back of list
			if (gl->eq.back) {
				gl->eq.back->next = tmp;
				gl->eq.back = tmp;
			} else {
				gl->eq.head = gl->eq.back = tmp;
			}
			break;
		case 0:
			// gthread returned. free.
			free(tmp);
			gthr_free(gt);
			free(gt);
			break;
		default:
			// gthread blocks. free tmp only.
			free(tmp);
		}
	}
}

int gthr_loop_wakeups(gthr_loop *gl) {
	if (gl->sleep.len == 0) return gl->minto;
	gthr *tgt;
	struct timespec now, cur;
	int msmin = gl->minto;
	clock_gettime(CLOCK_MONOTONIC, &now);
	for (int i = 0; i < gl->sleep.len; i++) {
		cur = gl->sleep.arr[i]->time;
		int ms = (cur.tv_sec - now.tv_sec) * 1000 + (cur.tv_nsec - now.tv_nsec) / 1000000;

		if (ms <= 0) {
			// swap [i] to end then add to exec queue.
			tgt = gl->sleep.arr[i];
			gl->sleep.arr[i] = gl->sleep.arr[gl->sleep.len - 1];
			gthrpv_pop(&gl->sleep);
			gthrpll_insert_back(&gl->eq, tgt);
		} else if (ms < msmin) {
			msmin = ms;
		}
	}
	if (msmin < 0) msmin = 0;
	return msmin;
}

void gthr_loop_poll(gthr_loop *gl, int timeout) {
	int rc = poll(gl->pfds.arr, gl->pfds.len, timeout);
	if (rc < 1) return; // check to see how to handle this

	pollfd tpfd;
	gthrp *tgt;
	for (int i = 0; i < gl->pfds.len; i++) {
		if (gl->pfds.arr[i].revents & POLLIN || gl->pfds.arr[i].revents & POLLOUT) {
			// swap to end and pop. add gt to list
			gl->pfds.arr[i] = gl->pfds.arr[gl->pfds.len - 1];
			pollfdv_pop(&gl->pfds);

			tgt = gl->gts.arr[i];
			gl->gts.arr[i] = gl->gts.arr[gl->gts.len - 1];
			gthrpv_pop(&gl->gts);
			gthrpll_insert_back(&gl->eq, tgt);
			i--;
		} else {
			// handle the other events, notably errors etc.
		}
	}
}

void gthr_loop_run(gthr_loop *gl) {
	gthr_loop_next(gl);
	int timeout = gthr_loop_wakeups(gl);
	// printf("%d\n", timeout);
	gthr_loop_poll(gl, gl->eq.head ? 0 : timeout);
}

void gthr_create(gthr_loop *gl, void (*fun)(void*, void*), void *args) {
	gthr *gt = malloc(sizeof(gthr));
	gthr_init(gt, 16 * 1024);
	gt->gl = gl;
	gt->ucp.uc_link = &gl->ucp;
	gt->fun = fun;
	gt->args = args;
	makecontext(&gt->ucp, (void (*)(void)) &gthr_wrap, 1, gt);
	gthrpll_insert_back(&gl->eq, gt);
}