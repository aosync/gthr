#include "gthr.h"

static void *
grow(void *arr, size_t len, size_t *cap, size_t elsize)
{
	if(len >= *cap){
		*cap *= 2;
		arr = realloc(arr, *cap * elsize);
	}
	return arr;
}

static void *
shrink(void *arr, size_t len, size_t *cap, size_t elsize)
{
	if(len < *cap / 2){
		*cap /= 2;
		arr = realloc(arr, *cap * elsize);
	}
	return arr;
}

static void
gthr_loop_list_append(struct gthr_loop *gl, struct gthr *gt)
{
	if(!gl->head)
		gl->head = gl->tail = gt;
	else{
		gl->tail->next = gt;
		gl->tail = gt;
	}
}

static struct gthr *
gthr_loop_list_get(struct gthr_loop *gl)
{
	struct gthr *res;
	if(!gl->head)
		return NULL;
	else{
		res = gl->head;
		gl->head = res->next;
		if(!gl->head)
			gl->tail = NULL;
		res->next = NULL;
	}
	return res;
}

_Thread_local struct gthr_loop	*_gthr_loop	= NULL;
_Thread_local struct gthr		*_gthr		= NULL;

void
gthr_create(void (*fun)(void*), void *args)
{
	gthr_create_on(_gthr_loop, fun, args);
}

void
gthr_create_on(struct gthr_loop *gl, void (*fun)(void*), void *args)
{
	struct gthr *gt = malloc(sizeof(struct gthr));
	gthr_init(gt, 16 * 1024);
	gt->gl = gl;
	gt->ucp.uc_link = &gl->ucp;
	gt->fun = fun;
	gt->args = args;
	makecontext(&gt->ucp, (void (*)(void)) &gthr_loop_wrap, 1, gt);
	gthr_loop_list_append(gl, gt);
}

int
gthr_init(struct gthr *gt, size_t size)
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
	gt->next = NULL;
	return 1;
}

void
gthr_destroy(struct gthr *gt)
{
	free(gt->sdata);
	free(gt);
}

void
gthr_loop_wrap(struct gthr *gt)
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
gthr_wait_pollfd(struct pollfd pfd)
{
	_gthr_loop->pfd[_gthr_loop->pfdl++] = pfd;
	_gthr_loop->pfd = grow(_gthr_loop->pfd, _gthr_loop->pfdl, &_gthr_loop->pfdc, sizeof(struct pollfd));
	_gthr_loop->inpoll[_gthr_loop->inpolll++] = _gthr;
	_gthr_loop->inpoll = grow(_gthr_loop->inpoll, _gthr_loop->inpolll, &_gthr_loop->inpollc, sizeof(struct gthr *));
	_gthr->ystat = GTHR_LAISSEZ;
	swapcontext(&_gthr->ucp, &_gthr_loop->ucp);
	return _gthr->werr;
}

int
gthr_wait_readable(int fd)
{
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;
	return gthr_wait_pollfd(pfd);
}

int
gthr_wait_writeable(int fd)
{
	struct pollfd pfd;
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

	_gthr_loop->sleep[_gthr_loop->sleepl++] = _gthr;
	_gthr_loop->sleep = grow(_gthr_loop->sleep, _gthr_loop->sleepl, &_gthr_loop->sleepc, sizeof(struct gthr *));

	_gthr->ystat = GTHR_LAISSEZ;
	swapcontext(&_gthr->ucp, &_gthr_loop->ucp);
}

void
gthr_loop_init(struct gthr_loop *gl)
{
	gl->minto = 2000;

	gl->pfd = malloc(sizeof(struct pollfd));
	gl->pfdl = 0;
	gl->pfdc = 1;
	gl->inpoll = malloc(sizeof(struct gthr *));
	gl->inpolll = 0;
	gl->inpollc = 1;

	gl->sleep = malloc(sizeof(struct gthr *));
	gl->sleepl = 0;
	gl->sleepc = 1;

	gl->head = gl->tail = NULL;
}

void
gthr_loop_finish(struct gthr_loop *gl)
{
	int i;
	struct gthr *next;
	for(i = 0; i < gl->inpolll; i++)
		free(gl->inpoll[i]);

	free(gl->pfd);
	free(gl->inpoll);

	for(i = 0; i < gl->sleepl; i++)
		free(gl->sleep[i]);

	free(gl->sleep);
	
	while((next = gthr_loop_list_get(gl)) != NULL){
		free(next);
	}
}

void
gthr_loop_do()
{
	struct gthr *gt;

	if((gt = gthr_loop_list_get(_gthr_loop)) == NULL)
		return;

	gt->ystat = GTHR_RETURN;
	_gthr = gt;
	swapcontext(&_gthr_loop->ucp, &_gthr->ucp);
	_gthr = NULL;
	gt->werr = 0;

	switch(gt->ystat) {
	case GTHR_YIELD:
		// gthread yielded. append to back of list
		gthr_loop_list_append(_gthr_loop, gt);
		break;
	case GTHR_RETURN:
		// gthread returned. free.
		gthr_destroy(gt);
		break;
	default:
		// gthread blocks
		/* nothing to do here */
		break;
	}
}

int
gthr_loop_wakeup()
{
	if (_gthr_loop->sleepl == 0) return -1;
	struct gthr *tgt;
	struct timespec now, cur;
	int msmin = _gthr_loop->minto, ms, one = 0;
	clock_gettime(CLOCK_MONOTONIC, &now);
	for (int i = 0; i < _gthr_loop->sleepl; i++) {
		cur = _gthr_loop->sleep[i]->time;
		ms = (cur.tv_sec - now.tv_sec) * 1000 + (cur.tv_nsec - now.tv_nsec) / 1000000;

		if (ms <= 0) {
			// swap [i] to end then add to exec queue.
			tgt = _gthr_loop->sleep[i];

			_gthr_loop->sleep[i] = _gthr_loop->sleep[_gthr_loop->sleepl - 1];

			_gthr_loop->sleepl--;
			_gthr_loop->sleep = shrink(_gthr_loop->sleep, _gthr_loop->sleepl, &_gthr_loop->sleepc, sizeof(struct gthr *));

			gthr_loop_list_append(_gthr_loop, tgt);
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
	int rc = poll(_gthr_loop->pfd, _gthr_loop->pfdl, timeout);
	if (rc < 1) return; // check to see how to handle this

	struct pollfd tpfd;
	struct gthr *tgt;
	for (int i = 0; i < _gthr_loop->pfdl; i++) {
		// if pollfd not triggered, continue
		if (!_gthr_loop->pfd[i].revents) continue;

		// if error is present, set werr flag to indicate error
		if (_gthr_loop->pfd[i].revents & POLLERR || _gthr_loop->pfd[i].revents & POLLNVAL)
			_gthr_loop->inpoll[i]->werr = -1;

		// in any case when pollfd is triggered, get rid of it (swap with end, and pop)
		_gthr_loop->pfd[i] = _gthr_loop->pfd[_gthr_loop->pfdl - 1];
		_gthr_loop->pfdl--;
		_gthr_loop->pfd = shrink(_gthr_loop->pfd, _gthr_loop->pfdl, &_gthr_loop->pfdc, sizeof(struct pollfd));

		// same thing for the corresponding gthread, but put it to run
		tgt = _gthr_loop->inpoll[i];
		_gthr_loop->inpoll[i] = _gthr_loop->inpoll[_gthr_loop->inpolll - 1]; /* already decremented */
		_gthr_loop->inpolll--;
		_gthr_loop->inpoll = shrink(_gthr_loop->inpoll, _gthr_loop->inpolll, &_gthr_loop->inpollc, sizeof(struct gthr *));

		gthr_loop_list_append(_gthr_loop, tgt);
		i--;
	}
}

void
gthr_loop_run(struct gthr_loop *gl)
{
	_gthr_loop = gl;

	gthr_loop_do();
	int timeout = gthr_loop_wakeup();
	gthr_loop_poll(_gthr_loop->head ? 0 : timeout);

	_gthr_loop = NULL;
}