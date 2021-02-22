#include "gthr.h"

_Thread_local gthr_loop	*_gthr_loop	= NULL;
_Thread_local gthr		*_gthr		= NULL;

void
gthr_create(void (*fun)(void*), void *args)
{
	gthr_create_on(_gthr_loop, fun, args);
}

void
gthr_create_on(gthr_loop *gl, void (*fun)(void*), void *args)
{
	gthr *gt = malloc(sizeof(gthr));
	gthr_init(gt, 16 * 1024);
	gt->gl = gl;
	gt->ucp.uc_link = &gl->ucp;
	gt->fun = fun;
	gt->args = args;
	makecontext(&gt->ucp, (void (*)(void)) &gthr_loop_wrap, 1, gt);
	gthrpll_insert_back(&gl->eq, gt);
}

int
gthr_init(gthr *gt, size_t size)
{
	gt->gl = NULL;
	gt->args = NULL;
	gt->werr = 0;
	getcontext(&gt->ucp);
	gt->sdata = malloc(size * sizeof(char));
	if (!gt->sdata)
		return 0;
	gt->ucp.uc_stack.ss_sp = gt->sdata;
	gt->ucp.uc_stack.ss_size = size * sizeof(char);
	return 1;
}

void
gthr_destroy(gthr *gt)
{
	free(gt->sdata);
	free(gt);
}

void
gthr_loop_wrap(gthr *gt)
{
	gt->fun(gt->args);
}

void
gthr_yield()
{
	_gthr->ystat = GTHR_YIELD;
	swapcontext(&_gthr->ucp, &_gthr_loop->ucp);
}

int
gthr_wait_pollfd(pollfd pfd)
{
	pollfdv_push(&_gthr_loop->pfds, pfd);
	gthrpv_push(&_gthr_loop->gts, _gthr);
	_gthr->ystat = GTHR_LAISSEZ;
	swapcontext(&_gthr->ucp, &_gthr_loop->ucp);
	return _gthr->werr;
}

int
gthr_wait_readable(int fd)
{
	pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;
	return gthr_wait_pollfd(pfd);
}

int
gthr_wait_writeable(int fd)
{
	pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLOUT;
	return gthr_wait_pollfd(pfd);
}

ssize_t
gthr_read(int fd, void *buf, size_t count)
{
	if (gthr_wait_readable(fd)) return -1;
	return read(fd, buf, count);
}

ssize_t
gthr_recv(int sockfd, void *buf, size_t length, int flags)
{
	if (gthr_wait_readable(sockfd)) return -1;
	return recv(sockfd, buf, length, flags);
}

ssize_t
gthr_write(int fd, void *buf, size_t count)
{
	if (gthr_wait_writeable(fd)) return -1;
	return write(fd, buf, count);
}

ssize_t
gthr_send(int sockfd, void *buf, size_t length, int flags)
{
	if (gthr_wait_writeable(sockfd)) return -1;
	return send(sockfd, buf, length, flags);
}

void
gthr_delay(long ms)
{
	clock_gettime(CLOCK_MONOTONIC, &_gthr->time);
	_gthr->time.tv_sec += ms / 1000;
	_gthr->time.tv_nsec += (ms * 1000000) % 1000000000;
	gthrpv_push(&_gthr_loop->sleep, _gthr);
	_gthr->ystat = GTHR_LAISSEZ;
	swapcontext(&_gthr->ucp, &_gthr_loop->ucp);
}

void
gthr_loop_init(gthr_loop *gl)
{
	gl->minto = 2000;
	gthrpv_init(&gl->gts);
	gthrpv_init(&gl->sleep);
	pollfdv_init(&gl->pfds);
	gthrpll_init(&gl->eq);
}

void
gthr_loop_do()
{
	if (_gthr_loop->eq.head) {
		gthrpll_ *tmp = _gthr_loop->eq.head;
		_gthr_loop->eq.head = tmp->next;
		if (!tmp->next) _gthr_loop->eq.back = tmp->next;

		gthr *gt = tmp->val;

		gt->ystat = GTHR_RETURN;
		_gthr = gt;
		swapcontext(&_gthr_loop->ucp, &_gthr->ucp);
		_gthr = NULL;
		gt->werr = 0;

		switch(gt->ystat) {
		case GTHR_YIELD:
			// gthread yielded. append to back of list
			if (_gthr_loop->eq.back) {
				_gthr_loop->eq.back->next = tmp;
				_gthr_loop->eq.back = tmp;
			} else {
				_gthr_loop->eq.head = _gthr_loop->eq.back = tmp;
			}
			break;
		case GTHR_RETURN:
			// gthread returned. free.
			free(tmp);
			gthr_destroy(gt);
			break;
		default:
			// gthread blocks. free tmp only.
			free(tmp);
		}
	}
}

int
gthr_loop_wakeup()
{
	if (_gthr_loop->sleep.len == 0) return -1;
	gthr *tgt;
	struct timespec now, cur;
	int msmin = _gthr_loop->minto, ms, one = 0;
	clock_gettime(CLOCK_MONOTONIC, &now);
	for (int i = 0; i < _gthr_loop->sleep.len; i++) {
		cur = _gthr_loop->sleep.arr[i]->time;
		ms = (cur.tv_sec - now.tv_sec) * 1000 + (cur.tv_nsec - now.tv_nsec) / 1000000;

		if (ms <= 0) {
			// swap [i] to end then add to exec queue.
			tgt = _gthr_loop->sleep.arr[i];
			_gthr_loop->sleep.arr[i] = _gthr_loop->sleep.arr[_gthr_loop->sleep.len - 1];
			gthrpv_pop(&_gthr_loop->sleep);
			gthrpll_insert_back(&_gthr_loop->eq, tgt);
			i--;
		} else if (ms <= msmin) {
			one = 1;
			msmin = ms;
		}
	}
	if (!one) msmin = -1;
	return msmin;
}

void
gthr_loop_poll(int timeout)
{
	int rc = poll(_gthr_loop->pfds.arr, _gthr_loop->pfds.len, timeout);
	if (rc < 1) return; // check to see how to handle this

	pollfd tpfd;
	gthrp *tgt;
	for (int i = 0; i < _gthr_loop->pfds.len; i++) {
		// if pollfd not triggered, continue
		if (!_gthr_loop->pfds.arr[i].revents) continue;

		// if error is present, set werr flag to indicate error
		if (_gthr_loop->pfds.arr[i].revents & POLLERR || _gthr_loop->pfds.arr[i].revents & POLLNVAL)
			_gthr_loop->gts.arr[i]->werr = 1;

		// in any case when pollfd is triggered, get rid of it (swap with end, and pop)
		_gthr_loop->pfds.arr[i] = _gthr_loop->pfds.arr[_gthr_loop->pfds.len - 1];
		pollfdv_pop(&_gthr_loop->pfds);

		// same thing for the corresponding gthread, but put it to run
		tgt = _gthr_loop->gts.arr[i];
		_gthr_loop->gts.arr[i] = _gthr_loop->gts.arr[_gthr_loop->gts.len - 1];
		gthrpv_pop(&_gthr_loop->gts);
		gthrpll_insert_back(&_gthr_loop->eq, tgt);
		i--;
	}
}

void
gthr_loop_run(gthr_loop *gl)
{
	_gthr_loop = gl;

	gthr_loop_do();
	int timeout = gthr_loop_wakeup();
	gthr_loop_poll(_gthr_loop->eq.head ? 0 : timeout);

	_gthr_loop = NULL;
}
