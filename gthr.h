#ifndef GTHR_H
#define GTHR_H

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

static int
gthr_init(gthr *gt, size_t size)
{
	gt->gl = NULL;
	gt->args = NULL;
	gt->wnum = 0;
	getcontext(&gt->ucp);
	gt->sdata = malloc(size * sizeof(char));
	if (!gt->sdata)
		return 0;
	gt->ucp.uc_stack.ss_sp = gt->sdata;
	gt->ucp.uc_stack.ss_size = size * sizeof(char);
	return 1;
}

static void
gthr_free(gthr *gt)
{
	free(gt->sdata);
	// free gt args too ? no! it's the gthread's role
}

static void
gthr_yield(gthr *gt)
{
	gt->snum = -1;
	swapcontext(&gt->ucp, &gt->gl->ucp);
}

static int
gthr_wait_pollfd(gthr *gt, pollfd pfd)
{
	pollfdv_push(&gt->gl->pfds, pfd);
	gthrpv_push(&gt->gl->gts, gt);
	gt->snum = 1;
	swapcontext(&gt->ucp, &gt->gl->ucp);
	return gt->wnum;
}

static void
gthr_delay(gthr *gt, long ms)
{
	clock_gettime(CLOCK_MONOTONIC, &gt->time);
	gt->time.tv_sec += ms / 1000;
	gt->time.tv_nsec += (ms * 1000000) % 1000000000;
	gthrpv_push(&gt->gl->sleep, gt);
	gt->snum = 1;
	swapcontext(&gt->ucp, &gt->gl->ucp);
}

static void
gthr_loop_init(gthr_loop *gl)
{
	gl->minto = 2000;
	gthrpv_init(&gl->gts);
	gthrpv_init(&gl->sleep);
	pollfdv_init(&gl->pfds);
	gthrpll_init(&gl->eq);
}

static void
gthr_wrap(gthr *gt)
{
	gt->fun(gt, gt->args);
}

static void
gthr_loop_next(gthr_loop *gl)
{
	if (gl->eq.head) {
		gthrpll_ *tmp = gl->eq.head;
		gl->eq.head = tmp->next;
		if (!tmp->next) gl->eq.back = tmp->next;

		gthr *gt = tmp->val;

		gt->snum = 0;
		swapcontext(&gl->ucp, &gt->ucp);
		gt->wnum = 0;

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

static int
gthr_loop_wakeups(gthr_loop *gl)
{
	if (gl->sleep.len == 0) return -1;
	gthr *tgt;
	struct timespec now, cur;
	int msmin = gl->minto, ms, one = 0;
	clock_gettime(CLOCK_MONOTONIC, &now);
	for (int i = 0; i < gl->sleep.len; i++) {
		cur = gl->sleep.arr[i]->time;
		ms = (cur.tv_sec - now.tv_sec) * 1000 + (cur.tv_nsec - now.tv_nsec) / 1000000;

		if (ms <= 0) {
			// swap [i] to end then add to exec queue.
			tgt = gl->sleep.arr[i];
			gl->sleep.arr[i] = gl->sleep.arr[gl->sleep.len - 1];
			gthrpv_pop(&gl->sleep);
			gthrpll_insert_back(&gl->eq, tgt);
			i--;
		} else if (ms < msmin) {
			one = 1;
			msmin = ms;
		}
	}
	if (!one) msmin = -1;
	return msmin;
}

static void
gthr_loop_poll(gthr_loop *gl, int timeout)
{
	int rc = poll(gl->pfds.arr, gl->pfds.len, timeout);
	if (rc < 1) return; // check to see how to handle this

	pollfd tpfd;
	gthrp *tgt;
	for (int i = 0; i < gl->pfds.len; i++) {
		// if pollfd not triggered, continue
		if (!gl->pfds.arr[i].revents) continue;

		// if error is present, set wnum flag to indicate error
		if (gl->pfds.arr[i].revents & POLLERR || gl->pfds.arr[i].revents & POLLNVAL)
			gl->gts.arr[i]->wnum = 1;

		// in any case when pollfd is triggered, get rid of it (swap with end, and pop)
		gl->pfds.arr[i] = gl->pfds.arr[gl->pfds.len - 1];
		pollfdv_pop(&gl->pfds);

		// same thing for the corresponding gthread, but put it to run
		tgt = gl->gts.arr[i];
		gl->gts.arr[i] = gl->gts.arr[gl->gts.len - 1];
		gthrpv_pop(&gl->gts);
		gthrpll_insert_back(&gl->eq, tgt);
		i--;
	}
}

static void
gthr_loop_run(gthr_loop *gl)
{
	gthr_loop_next(gl);

	int timeout = gthr_loop_wakeups(gl);

	gthr_loop_poll(gl, gl->eq.head ? 0 : timeout);
}

static void
gthr_create(gthr_loop *gl, void (*fun)(void*, void*), void *args)
{
	gthr *gt = malloc(sizeof(gthr));
	gthr_init(gt, 16 * 1024);
	gt->gl = gl;
	gt->ucp.uc_link = &gl->ucp;
	gt->fun = fun;
	gt->args = args;
	makecontext(&gt->ucp, (void (*)(void)) &gthr_wrap, 1, gt);
	gthrpll_insert_back(&gl->eq, gt);
}

#endif