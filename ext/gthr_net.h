#ifndef GTHR_SOCK_H
#define GTHR_SOCK_H

/*
	gthr_net.h:
	Convenience functions to access the internet
*/

#include "gthr.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>


/*
	this uses the blocking :( getaddrinfo()
	getaddrinfo_a exists but it's a GNU extension.
*/
static int
gthr_lookup(char *address, struct in_addr *addr) {
	struct addrinfo hints, *res, *tmp;
	int rc, one = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;

	if (getaddrinfo(address, NULL, &hints, &tmp))
		return -1;

	res = tmp;
	while (res) {
		if (res->ai_family) {
			*addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
			one = 0;
			break;
		}
		res = res->ai_next;
	}

	freeaddrinfo(tmp);

	return one;
}


static int
gthr_tcpdial(char *address, int port) {
	int sockfd;
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, address, &addr.sin_addr) <= 0) {
		if (gthr_lookup(address, &addr.sin_addr))
			return -1;
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	fcntl(sockfd, F_SETFL, O_NONBLOCK);
	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) >= 0)
		return -1;
	if (errno != EINPROGRESS)
		return -1;
	if (gthr_wait_writeable(sockfd))
	  	goto nosocket;
	
	return sockfd;
nosocket:
	close(sockfd);
	return -1;
}

#endif