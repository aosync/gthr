#include "gthr.h"

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

		.bin_lock = spin_new(NULL)
	};
}

void
gthr_context_finish(struct gthr_context *gctx)
{
	/* locking is not needed here but i still do it */
	spin(void *_, &gctx->exqueue_lock){
		while(gctx->exqueue_head)
			gthr_free(gthr_context_exqueue_recv(gctx));
	}
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
