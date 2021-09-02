#ifndef GTHR_H
#define GTHR_H

#include <vec.h>
#include <ctx.h>

#include <poll.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>

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
	_Atomic char runs;
};

struct gthr *gthr_make(struct gthr_context *, size_t);
void gthr_free(struct gthr *);

void gthr_yield(void);

#define GTHR_BIN_CAP 64
#define GTHR_THRD_CAP 64

struct gthr_context{
	pthread_mutex_t exqueue_lock;
	struct gthr  *exqueue_head;
	struct gthr  *exqueue_tail;

	pthread_mutex_t sleep_lock;
	vec(struct gthr *) sleep;

	pthread_mutex_t plist_lock;
	vec(struct pollfd) plist_pfd;
	vec(struct gthr *) plist_gthr;

	pthread_mutex_t bin_lock;
	struct gthr *bin[GTHR_BIN_CAP];
	unsigned bin_len;
	
	/* [ ... probably threading stuff ] */
	unsigned thrds_len;
	pthread_t thrds[GTHR_THRD_CAP];

	pthread_cond_t exqueue_new;
	
	_Atomic char running;
	_Atomic unsigned runners;
	_Atomic unsigned sema;
};

extern _Thread_local volatile struct gthr *_gthr_;
extern _Thread_local volatile struct gthr_context *_gthr_context_;


void gthr_context_init(struct gthr_context *);
void gthr_context_finish(struct gthr_context *);

struct gthr *gthr_context_exqueue_recv(struct gthr_context *);
void gthr_context_exqueue_send(struct gthr_context *, struct gthr *);

char gthr_context_run_once(struct gthr_context *);
void gthr_context_run(struct gthr_context *);
void gthr_context_runners(struct gthr_context *, unsigned);
void gthr_context_end_runners(struct gthr_context *);

// 

void _gthr_wrap(void);
char gthr_create(void (*)(void*), void *);
char gthr_create_on(struct gthr_context *, void (*)(void*), void *);
void gthr_recycle(struct gthr *);

#endif
