#ifndef GTHR_CURL_H
#define GTHR_CURL_H

#include "gthr.h"

#include <curl/curl.h>
#include <curl/multi.h>
#include <sys/types.h>
#include <poll.h>

static void
gthr_curl_perform(CURL *handle) {
	CURLM *multi_handle;
	CURLMsg *msg;
	int running = 0;

	multi_handle = curl_multi_init();
	curl_multi_add_handle(multi_handle, handle);

	curl_multi_perform(multi_handle, &running);

	while (running) {
		int maxfd = -1;
		CURLMcode mc;
		fd_set fdread;
		fd_set fdwrite;
		fd_set fdexcep;
		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);
		mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);
		if (mc != CURLM_OK)
			break;
		if (maxfd == -1) {
			gthr_delay(100);
		} else {
			if (FD_ISSET(maxfd, &fdread)) {
				if (gthr_wait_readable(maxfd)) break;
			} else if (FD_ISSET(maxfd, &fdwrite)) {
				if (gthr_wait_writeable(maxfd)) break;
			}
		}
		curl_multi_perform(multi_handle, &running);
	}
	curl_multi_cleanup(multi_handle);
}

#endif
