#include <stdlib.h>
#include <ucontext.h>
#include <stdio.h>
#include <poll.h>

typedef struct pollfd pollfd;
typedef struct gthr_loop gthr_loop;
typedef struct gthr gthr;

struct gthr {
	char 				*sdata;
	ucontext_t 			ucp;
	short				snum;
	struct gthr_loop 	*gl;
	struct pollfd		*curpfd;
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
	gthrpll		eq;
};

int gthr_init(gthr *gt, size_t size) {
	gt->gl = NULL;
	gt->curpfd = NULL;
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

void gthr_loop_init(gthr_loop *gl) {
	gthrpv_init(&gl->gts);
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
		gt->curpfd = NULL;

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

void gthr_loop_poll(gthr_loop *gl, int timeout) {
	int rc = poll(gl->pfds.arr, gl->pfds.len, timeout);
	if (rc < 1) return; // check to see how to handle this

	pollfd tpfd;
	gthrp *tgt;
	for (int i = 0; i < gl->pfds.len; i++) {
		if (gl->pfds.arr[i].revents & POLLIN || gl->pfds.arr[i].revents & POLLOUT) {
			// swap to end and pop. add gt to list
			tpfd = gl->pfds.arr[i];
			gl->pfds.arr[i] = gl->pfds.arr[gl->pfds.len - 1];
			gl->pfds.arr[gl->pfds.len - 1] = tpfd;
			pollfdv_pop(&gl->pfds);
			tgt = gl->gts.arr[i];
			gl->gts.arr[i] = gl->gts.arr[gl->gts.len - 1];
			gl->gts.arr[gl->gts.len - 1] = tgt;
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
	
	gthr_loop_poll(gl, gl->eq.head ? 0 : 2000);
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