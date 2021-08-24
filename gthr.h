#ifndef GTHR_H
#define GTHR_H

#include <vec.h>
#include <ctx.h>
#include <spin.h>

#include <poll.h>
#include <unistd.h>
#include <sys/mman.h>

enum gthr_yield_status {
	GTHR_RETURN,
	GTHR_LAISSEZ
};

struct gthr{
	struct gthr_context *context;
	struct gthr *next;
	
	void (*function)(void*);
	void *args;
	
	void *stack_data;
	size_t stack_size;

	struct ctx ctx, link;

	enum gthr_yield_status yield_status;
	short wake_errno;
	
	struct timespec wakeup_time;
};

struct gthr *gthr_make(struct gthr_context *, size_t);
void gthr_free(struct gthr *);

void gthr_yield(void);

#define GTHR_BIN_CAP 64

struct gthr_context{
	struct spin exqueue_lock;
	struct gthr  *exqueue_head;
	struct gthr  *exqueue_tail;

	struct spin sleep_lock;
	vec(struct gthr *) sleep;

	struct spin plist_lock;
	vec(struct pollfd) plist_pfd;
	vec(struct gthr *) plist_gthr;

	struct spin bin_lock;
	struct gthr *bin[GTHR_BIN_CAP];
	unsigned bin_len;
	
	/* [ ... probably threading stuff ] */
	_Atomic char running;
	_Atomic unsigned runners;
};

extern _Thread_local struct gthr *_gthr;
extern _Thread_local struct gthr_context *_gthr_context;


void gthr_context_init(struct gthr_context *);
void gthr_context_finish(struct gthr_context *);

struct gthr *gthr_context_exqueue_recv(struct gthr_context *);
void gthr_context_exqueue_send(struct gthr_context *, struct gthr *);

void gthr_context_run_once(struct gthr_context *);
void gthr_context_run(struct gthr_context *);

// 

void _gthr_wrap(void);
char gthr_create(void (*)(void*), void *);
char gthr_create_on(struct gthr_context *, void (*)(void*), void *);
void gthr_recycle(struct gthr *);

#endif
