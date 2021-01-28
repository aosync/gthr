#ifndef GTHR_SSL_NET
#define GTHR_SSL_NET

#include "gthr.h"
#include "gthr_net.h"

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <errno.h>

struct gthr_ssl_socket {
	int			sockfd;
	SSL_CTX		*ctx;
	SSL			*ssl;
};

typedef struct gthr_ssl_socket gthr_ssl_socket;

/*
	compile with -lssl -lcrypto
*/

static void
gthr_ssl_init()
{
	OpenSSL_add_all_algorithms();
	ERR_load_BIO_strings();
	ERR_load_crypto_strings();
	SSL_load_error_strings();
	SSL_library_init();
}

static gthr_ssl_socket *
gthr_ssl_tcpdial(gthr *gt, char *address, struct in_addr *addr)
{
	gthr_ssl_socket *res;
	int sockfd = gthr_tcpdial(gt, address, addr), ret;
	if (sockfd < 0)
		return NULL;
	SSL_METHOD *method;
	SSL_CTX *ctx;
	SSL *ssl;
	method = SSLv23_client_method();
	ctx = SSL_CTX_new(method);
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	if (!ctx)
		return NULL;
	ssl = SSL_new(ctx);
	SSL_set_fd(ssl, sockfd);

	// this is needed for non-blocking sockets
	while (-1 == (ret = SSL_connect(ssl))) {
		switch(SSL_get_error(ssl, ret)) {
		case SSL_ERROR_WANT_READ:
			if (gthr_wait_readable(gt, sockfd))
				goto clean;
			break;
		case SSL_ERROR_WANT_WRITE:
			if (gthr_wait_writeable(gt, sockfd))
				goto clean;
			break;
		default:
			goto clean;
		}
	}

	res = malloc(sizeof(gthr_ssl_socket));
	res->sockfd = sockfd;
	res->ctx = ctx;
	res->ssl = ssl;

	return res;

clean:
	SSL_free(ssl);
	close(sockfd);
	SSL_CTX_free(ctx);
	return NULL;
}

static int
gthr_ssl_read(gthr *gt, gthr_ssl_socket *sock, void *buf, int count)
{
	int ret;
	while (-1 == (ret = SSL_read(sock->ssl, buf, count))) {
		switch(SSL_get_error(sock->ssl, ret)) {
		case SSL_ERROR_WANT_READ:
			if (gthr_wait_readable(gt, sock->sockfd))
				return -1;
			break;
		case SSL_ERROR_WANT_WRITE:
			if (gthr_wait_writeable(gt, sock->sockfd))
				return -1;
			break;
		default:
			return -1;
		}
	}
	return ret;
}

static int
gthr_ssl_write(gthr *gt, gthr_ssl_socket *sock, void *buf, int count)
{
	int ret;
	while (-1 == (ret = SSL_write(sock->ssl, buf, count))) {
		switch(SSL_get_error(sock->ssl, ret)) {
		case SSL_ERROR_WANT_READ:
			if (gthr_wait_readable(gt, sock->sockfd))
				return -1;
			break;
		case SSL_ERROR_WANT_WRITE:
			if (gthr_wait_writeable(gt, sock->sockfd))
				return -1;
			break;
		default:
			return -1;
		}
	}
	return ret;
}

static void
gthr_ssl_close(gthr *gt, gthr_ssl_socket *sock)
{
	SSL_free(sock->ssl);
	close(sock->sockfd);
	SSL_CTX_free(sock->ctx);
	free(sock);
}

#endif