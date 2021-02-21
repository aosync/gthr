#ifndef GTHR_SSL_NET
#define GTHR_SSL_NET

#include "gthr.h"
#include "gthr_net.h"

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <errno.h>

/*
	link with -lssl -lcrypto
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

static SSL *
gthr_ssl_tcpdial(char *address, struct in_addr *addr)
{
	int sockfd = gthr_tcpdial(address, addr), ret;
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
			if (gthr_wait_readable(sockfd))
				goto clean;
			break;
		case SSL_ERROR_WANT_WRITE:
			if (gthr_wait_writeable(sockfd))
				goto clean;
			break;
		default:
			goto clean;
		}
	}

	return ssl;

clean:
	SSL_free(ssl);
	close(sockfd);
	SSL_CTX_free(ctx);
	return NULL;
}

static int
gthr_ssl_read(SSL *sock, void *buf, int count)
{
	int ret;
	while (-1 == (ret = SSL_read(sock, buf, count))) {
		int sfd = SSL_get_fd(sock);
		switch(SSL_get_error(sock, ret)) {
		case SSL_ERROR_WANT_READ:
			if (gthr_wait_readable(sfd))
				return -1;
			break;
		case SSL_ERROR_WANT_WRITE:
			if (gthr_wait_writeable(sfd))
				return -1;
			break;
		default:
			return -1;
		}
	}
	return ret;
}

static int
gthr_ssl_write(SSL *sock, void *buf, int count)
{
	int ret;
	while (-1 == (ret = SSL_write(sock, buf, count))) {
		int sfd = SSL_get_fd(sock);
		switch(SSL_get_error(sock, ret)) {
		case SSL_ERROR_WANT_READ:
			if (gthr_wait_readable(sfd))
				return -1;
			break;
		case SSL_ERROR_WANT_WRITE:
			if (gthr_wait_writeable(sfd))
				return -1;
			break;
		default:
			return -1;
		}
	}
	return ret;
}

static void
gthr_ssl_close(SSL *sock)
{
	SSL_shutdown(sock);
	close(SSL_get_fd(sock));
	SSL_CTX_free(SSL_get_SSL_CTX(sock));
	SSL_free(sock);
}

#endif