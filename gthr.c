#include "gthr.h"

#ifdef __amd64__
#define STKTO(x) asm volatile ("mov %0, %%rsp" : : "r"(x))
#endif

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

static void
gthr_switch(struct gthr_jmp *from, struct gthr_jmp *to)
{
	if(!gthr_setjmp(from))
		gthr_longjmp(to, 1);
}

_Thread_local struct gthr_loop	*_gthr_loop	= NULL;
_Thread_local struct gthr		*_gthr		= NULL;

void
gthr_create(void (*fun)(void*), void *args)
{
	struct gthr *tmp = _gthr;
	struct gthr *gt = malloc(sizeof(struct gthr));
	gthr_init(gt, 8);
	void *end = gt->sdata + gt->ssize;
	gt->gl = _gthr_loop;
	gt->fun = fun;
	gt->args = args;
	_gthr = gt;
	if(!gthr_setjmp(&_gthr_loop->link)){
		STKTO(end);
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
gthr_destroy(struct gthr *gt)
{
	munmap(gt->sdata, gt->ssize);
	//free(gt->sdata);
	free(gt);
}

void
gthr_wrap()
{
	gthr_switch(&_gthr->jmp, &_gthr_loop->link);
	_gthr->fun(_gthr->args);
	gthr_switch(&_gthr->jmp, &_gthr_loop->loop);
}

void
gthr_yield()
{
	_gthr->ystat = GTHR_YIELD;
	gthr_switch(&_gthr->jmp, &_gthr_loop->loop);
}

int
gthr_wait_pollfd(struct pollfd pfd)
{
	_gthr_loop->pfd[_gthr_loop->pfdl++] = pfd;
	_gthr_loop->pfd = grow(_gthr_loop->pfd, _gthr_loop->pfdl, &_gthr_loop->pfdc, sizeof(struct pollfd));
	_gthr_loop->inpoll[_gthr_loop->inpolll++] = _gthr;
	_gthr_loop->inpoll = grow(_gthr_loop->inpoll, _gthr_loop->inpolll, &_gthr_loop->inpollc, sizeof(struct gthr *));
	_gthr->ystat = GTHR_LAISSEZ;
	gthr_switch(&_gthr->jmp, &_gthr_loop->loop);
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
	gthr_switch(&_gthr->jmp, &_gthr_loop->loop);
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
		gthr_destroy(gl->inpoll[i]);

	free(gl->pfd);
	free(gl->inpoll);

	for(i = 0; i < gl->sleepl; i++)
		gthr_destroy(gl->sleep[i]);

	free(gl->sleep);
	
	while((next = gthr_loop_list_get(gl)) != NULL)
		gthr_destroy(next);
}

void
gthr_loop_do()
{
	struct gthr *gt;

	if((gt = gthr_loop_list_get(_gthr_loop)) == NULL)
		return;

	gt->ystat = GTHR_RETURN;
	_gthr = gt;
	gthr_switch(&_gthr_loop->loop, &_gthr->jmp);
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

#if defined(__amd64__)
asm("gthr_setjmp:\n"
	"mov %rbx, (%rdi)\n"
	"lea 8(%rsp), %rcx\n"
	"mov %rcx, 8(%rdi)\n"
	"mov %rbp, 16(%rdi)\n"
	"mov %r12, 24(%rdi)\n"
	"mov %r13, 32(%rdi)\n"
	"mov %r14, 40(%rdi)\n"
	"mov %r15, 48(%rdi)\n"
	"mov (%rsp), %rcx\n"
	"mov %rcx, 56(%rdi)\n"
	"xor %rax, %rax\n"
	"ret\n"
	);
asm("gthr_longjmp:\n"
	"mov (%rdi), %rbx\n"
	"mov 8(%rdi), %rsp\n"
	"mov 16(%rdi), %rbp\n"
	"mov 24(%rdi), %r12\n"
	"mov 32(%rdi), %r13\n"
	"mov 40(%rdi), %r14\n"
	"mov 48(%rdi), %r15\n"
	"mov %rsi, %rax\n"
	"jmp *56(%rdi)\n"
	"ret\n");
#endif
