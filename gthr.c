#include "gthr.h"

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
	struct gthr *tmp = _gthr;
	struct gthr *gt;
	if(_gthr_loop->binl == 0){
		gt = malloc(sizeof(struct gthr));
		gthr_init(gt, 8);
	} else
		gt = _gthr_loop->bin[--_gthr_loop->binl];
	void *end = (void *)((size_t)(gt->sdata + gt->ssize - 1) & ~0xF); /* align properly */
	gt->gl = _gthr_loop;
	gt->fun = fun;
	gt->args = args;
	_gthr = gt;
	if(!ctx_save(&_gthr_loop->link)){
		ctx_stack_to(end);
		gthr_wrap();
	}
	gthr_loop_list_append(_gthr_loop, gt);
	_gthr = tmp;
}

void
gthr_create_on(struct gthr_loop *gl, void (*fun)(void*), void *args)
{
	struct gthr_loop *tmp = _gthr_loop;
	_gthr_loop = gl;
	gthr_create(fun, args);
	_gthr_loop = tmp;
}

int
gthr_init(struct gthr *gt, size_t size)
{
	gt->gl = NULL;
	gt->args = NULL;
	gt->werr = 0;
	long ps = sysconf(_SC_PAGESIZE);
	gt->ssize = (size+1)*ps;
	gt->sdata = mmap(NULL, gt->ssize, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE|MAP_STACK, -1, 0);
	if(gt->sdata == MAP_FAILED)
		return 0;
	gt->next = NULL;
	return 1;
}

void
gthr_finish(struct gthr *gt)
{
	munmap(gt->sdata, gt->ssize);
	free(gt);
}

void
gthr_destroy(struct gthr *gt)
{
	if(_gthr_loop->binl >= GTHR_BIN)
		gthr_finish(gt);
	else{
		gt->fun = NULL;
		gt->args = NULL;
		_gthr_loop->bin[_gthr_loop->binl++] = gt;
	}
}

void
gthr_wrap()
{
	ctx_switch(&_gthr->jmp, &_gthr_loop->link);
	_gthr->fun(_gthr->args);
	ctx_switch(&_gthr->jmp, &_gthr_loop->loop);
}

void
gthr_yield()
{
	_gthr->ystat = GTHR_YIELD;
	ctx_switch(&_gthr->jmp, &_gthr_loop->loop);
}

int
gthr_wait_pollfd(struct pollfd pfd)
{
	vec_push(_gthr_loop->pfd, pfd);
	vec_push(_gthr_loop->inpoll, _gthr);
	_gthr->ystat = GTHR_LAISSEZ;
	ctx_switch(&_gthr->jmp, &_gthr_loop->loop);
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

	vec_push(_gthr_loop->sleep, _gthr);

	_gthr->ystat = GTHR_LAISSEZ;
	ctx_switch(&_gthr->jmp, &_gthr_loop->loop);
}

void
gthr_loop_init(struct gthr_loop *gl)
{
	gl->minto = 2000;

	gl->pfd = vec_new(struct pollfd);
	gl->inpoll = vec_new(struct gthr *);

	gl->sleep = vec_new(struct gthr *);

	gl->binl = 0;

	gl->head = gl->tail = NULL;
}

void
gthr_loop_finish(struct gthr_loop *gl)
{
	int i;
	struct gthr *next;
	vec_foreach(struct gthr *, g, gl->inpoll) {
		gthr_finish(g);
	}

	free(gl->pfd);
	free(gl->inpoll);

	vec_foreach(struct gthr *, g, gl->sleep) {
		gthr_finish(g);
	}

	free(gl->sleep);

	for(i = 0; i < gl->binl; i++)
		gthr_finish(gl->bin[i]);

	gl->binl = 0;
	
	while((next = gthr_loop_list_get(gl)) != NULL)
		gthr_finish(next);
}

void
gthr_loop_do()
{
	struct gthr *gt;

	if((gt = gthr_loop_list_get(_gthr_loop)) == NULL)
		return;

	gt->ystat = GTHR_RETURN;
	_gthr = gt;
	ctx_switch(&_gthr_loop->loop, &_gthr->jmp);
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
	if (_vec_header(_gthr_loop->sleep)->len == 0) return -1;
	struct gthr *tgt;
	struct timespec now, cur;
	int msmin = _gthr_loop->minto, ms, one = 0;
	clock_gettime(CLOCK_MONOTONIC, &now);
	for (int i = 0; i < _vec_header(_gthr_loop->sleep)->len; i++) {
		cur = _gthr_loop->sleep[i]->time;
		ms = (cur.tv_sec - now.tv_sec) * 1000 + (cur.tv_nsec - now.tv_nsec) / 1000000;

		if (ms <= 0) {
			// swap [i] to end then add to exec queue.
			tgt = _gthr_loop->sleep[i];

			_gthr_loop->sleep[i] = _gthr_loop->sleep[_vec_header(_gthr_loop->sleep)->len - 1];

			vec_pop(_gthr_loop->sleep);

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
	int rc = poll(_gthr_loop->pfd, _vec_header(_gthr_loop->pfd)->len, timeout);
	if (rc < 1) return; // check to see how to handle this

	struct pollfd tpfd;
	struct gthr *tgt;
	for (int i = 0; i < _vec_header(_gthr_loop->pfd)->len; i++) {
		// if pollfd not triggered, continue
		if (!_gthr_loop->pfd[i].revents) continue;

		// if error is present, set werr flag to indicate error
		if (_gthr_loop->pfd[i].revents & POLLERR || _gthr_loop->pfd[i].revents & POLLNVAL)
			_gthr_loop->inpoll[i]->werr = -1;

		// in any case when pollfd is triggered, get rid of it (swap with end, and pop)
		_gthr_loop->pfd[i] = _gthr_loop->pfd[_vec_header(_gthr_loop->pfd)->len - 1];
		vec_pop(_gthr_loop->pfd);

		// same thing for the corresponding gthread, but put it to run
		tgt = _gthr_loop->inpoll[i];
		_gthr_loop->inpoll[i] = _gthr_loop->inpoll[_vec_header(_gthr_loop->inpoll)->len - 1]; /* already decremented */
		vec_pop(_gthr_loop->inpoll);

		gthr_loop_list_append(_gthr_loop, tgt);
		i--;
	}
}

char
gthr_loop_run(struct gthr_loop *gl)
{
	_gthr_loop = gl;

	gthr_loop_do();
	int timeout = gthr_loop_wakeup();
	if(_gthr_loop->head == NULL && _vec_header(_gthr_loop->sleep)->len == 0 && _vec_header(_gthr_loop->inpoll)->len == 0)
		return 0;
	gthr_loop_poll(_gthr_loop->head ? 0 : timeout);

	_gthr_loop = NULL;
	return 1;
}
