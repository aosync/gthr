#include "gthr.h"

_Thread_local volatile struct gthr *_gthr_ = NULL;
_Thread_local volatile struct gthr_context *_gthr_context_ = NULL;

#if defined(__clang__)
/* This is a workaround to force clang to treat thread local variables as volatile */
__attribute__((noinline)) struct gthr *
_get_gthr()
{
	return _gthr_;
}
__attribute__((noinline)) struct gthr_context *
_get_gthr_context()
{
	return _gthr_context_;
}

__attribute__((noinline)) void
_set_gthr(struct gthr* g)
{
	_gthr_ = g;
}
__attribute__((noinline)) void
_set_gthr_context(struct gthr_context *gctx)
{
	_gthr_context_ = gctx;
}

#define _gthr (_get_gthr())
#define _gthr_context (_get_gthr_context())
#else
#define _gthr (_gthr_)
#define _gthr_context (_gthr_context_)
#define _set_gthr(x) (_gthr_ = (x))
#define _set_gthr_context(x) (_gthr_context_ = (x))
#endif

struct gthr *
gthr_make(struct gthr_context *gctx, size_t stack_pages)
{
	size_t page_size = sysconf(_SC_PAGESIZE);
	size_t stack_size = (stack_pages + 1) * page_size;

	void *stack_data = mmap(NULL, stack_size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE|MAP_STACK, -1, 0);
	if(stack_data == MAP_FAILED)
		return NULL;
	
	struct gthr *gthr = malloc(sizeof(struct gthr));
	if(gthr == NULL){
		munmap(stack_data, stack_size);
		return NULL;
	}
	*gthr = (struct gthr) {
		.context = gctx,
		.stack_size = stack_size,
		.stack_data = stack_data		
	};

	return gthr;
}

void
gthr_free(struct gthr *g)
{
	munmap(g->stack_data, g->stack_size);
	free(g);
}

void
gthr_yield()
{
	gthr_context_exqueue_send(_gthr_context, _gthr);
	_gthr->yield_status = GTHR_LAISSEZ;
	ctx_switch(&_gthr->ctx, &_gthr->link);
}

//

void
gthr_context_init(struct gthr_context *gctx)
{
	*gctx = (struct gthr_context){
		.exqueue_lock = spin_new(NULL),

		.sleep_lock = spin_new(NULL),
		.sleep = vec_new(struct gthr *),

		.plist_lock = spin_new(NULL),
		.plist_pfd = vec_new(struct pollfd),
		.plist_gthr = vec_new(struct gthr *),

		.bin_lock = spin_new(NULL),

		.running = 1,
		.runners = 0,
	};
}

void
gthr_context_finish(struct gthr_context *gctx)
{
	/* locking is not needed here but i still do it */
	while(gctx->exqueue_head)
		gthr_free(gthr_context_exqueue_recv(gctx));
	spin(void *_, &gctx->sleep_lock){
		vec_foreach(struct gthr *, it, gctx->sleep){
			gthr_free(it);
		}
		vec_free(gctx->sleep);
	}
	spin(void *_, &gctx->plist_lock){
		vec_free(gctx->plist_pfd);
		vec_foreach(struct gthr *, it, gctx->plist_gthr){
			gthr_free(it);
		}
		vec_free(gctx->plist_gthr);
	}
	spin(void *_, &gctx->bin_lock){
		for(unsigned i = 0; i < gctx->bin_len; i++){
			gthr_free(gctx->bin[i]);
		}
	}
}

struct gthr *
gthr_context_exqueue_recv(struct gthr_context *gctx)
{
	struct gthr *g;
	spin(void *_, &gctx->exqueue_lock){
		g = gctx->exqueue_head;
		if(gctx->exqueue_head == gctx->exqueue_tail)
			gctx->exqueue_head = gctx->exqueue_tail = NULL;
		else
			gctx->exqueue_head = gctx->exqueue_head->next;
	}
	if(g != NULL)
		g->next = NULL;
	return g;
}

void
gthr_context_exqueue_send(struct gthr_context *gctx, struct gthr *g)
{
	spin(void *_, &gctx->exqueue_lock){
		if(gctx->exqueue_head == NULL)
			gctx->exqueue_head = gctx->exqueue_tail = g;
		else{
			gctx->exqueue_tail->next = g;
			gctx->exqueue_tail = g;
		}
	}
}

char
gthr_context_run_once(struct gthr_context *gctx)
{
	_set_gthr_context(gctx);

	_set_gthr(gthr_context_exqueue_recv(gctx));
	if(!_gthr)
		return 1;
	if(_gthr->runs){
		gthr_context_exqueue_send(gctx, _gthr);
		return 2;
	}

	_gthr->yield_status = GTHR_RETURN;
	_gthr->runs = 1;
	ctx_switch(&_gthr->link, &_gthr->ctx);
	_gthr->runs = 0;
	_gthr->wake_errno = 0;

	printf("%p in loop %d\n", _gthr, _gthr->yield_status);
	if(_gthr->yield_status == GTHR_RETURN)
		gthr_recycle(_gthr);

	_set_gthr(NULL);
	_set_gthr_context(NULL);
	return 0;
}

void
gthr_context_run(struct gthr_context *gctx)
{
	gctx->runners++;
	while(gctx->running){
		if(gthr_context_run_once(gctx))
			sleep(1);
		/* TODO ^^^^^^^^^ (important): implement this with a condition variable */
	}
	gctx->runners--;
}

void
gthr_context_runners(struct gthr_context *gctx, unsigned n)
{
	if(n >= GTHR_THRD_CAP)
		return;
	gctx->thrds_len = n;
	for(unsigned i = 0; i < n; i++)
		pthread_create(&gctx->thrds[i], NULL, gthr_context_run, gctx);
}

void
gthr_context_end_runners(struct gthr_context *gctx)
{
	gctx->running = 0;
	for(unsigned i = 0; i < gctx->thrds_len; i++)
		pthread_join(gctx->thrds[i], NULL);
}


//

void
_gthr_wrap()
{
	ctx_switch(&_gthr->ctx, &_gthr->link);

	_gthr->function(_gthr->args);

	printf("%p in wrap %d\n", _gthr, _gthr->yield_status);
	ctx_switch(&_gthr->ctx, &_gthr->link);
	// unreachable
}

char
gthr_create(void (*function)(void*), void *args)
{
	struct gthr *_prev = _gthr;
	struct gthr *g = NULL;
	spin(void *_, &_gthr_context->bin_lock){
		if(_gthr_context->bin_len > 0)
			g = _gthr_context->bin[--_gthr_context->bin_len];
	}
	if(g == NULL)
		g = gthr_make(_gthr_context, 8);
	if(g == NULL)
		return 0;

	g->function = function;
	g->args = args;

	void *stack_end = (void *)((size_t)(g->stack_data + g->stack_size - 1) & ~0xF);

	_set_gthr(g);
	if(!ctx_save(&_gthr->link)){
		ctx_stack_to(stack_end);
		_gthr_wrap();
		// unreachable
	}
	//_gthr = _prev;

	gthr_context_exqueue_send(_gthr_context, g);
	return 1;
}

char
gthr_create_on(struct gthr_context *gctx, void (*function)(void*), void *args)
{
	char rc;
	struct gthr_context *_prev = _gthr_context;
	_set_gthr_context(gctx);
	rc = gthr_create(function, args);
	_set_gthr_context(_prev);
	return rc;
}

void
gthr_recycle(struct gthr *g)
{
	char binned = 0;
	spin(void *_, &_gthr_context->bin_lock){
		if(_gthr_context->bin_len < GTHR_BIN_CAP){
			_gthr_context->bin[_gthr_context->bin_len++] = g;
			binned = 1;
		}
	}
	if(!binned)
		gthr_free(g);
}
