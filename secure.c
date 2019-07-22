/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 * 
 * [any text provided by the client]
 *
 * secure.c
 *      Urmet IoT ssl/tls and cert management implementation
 *
 * Authors:
 *      Matteo Di Leo <matteo.dileo@csp.it>
 */

#include "secure.h"

//#ifndef IOTC_CLIENT

#include <openssl/conf.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <unistd.h>
#include <glib/glib.h>
#include <gio/gio.h>

struct httpsCtx {
	int sock;
	SSL_CTX *sslCtx;
	SSL *ssl;
	GSocketClient *gSocketClient;
	GSocketConnection *gSocketConnection;
	char *host;
	unsigned short port;
	char *path;
	char *CAFile;
	char *CAPath;
	char *crtFile;
	char *keyFile;
	char *postMsg;
	void (*callback)(int, char *, void*);
	void *userData;
};

/**
 * @brief A structure used to exchange x509 certificates data
 *
 * This structure contains private key of node, eventually the csr used to request a certificate,
 * and eventually the certificate of node signed by CA.
 * @see certBundleFree()
 * @note All fields should be initialized to NULL.
 * @note Deallocation of structure should be done invoking certBundleFree()
 */
struct certBundle {
	EVP_PKEY *pkey; /**< The private Key used to sign messages */
	X509_REQ *csr;  /**< The Certificate Signing Request related to my key */
	X509 *cert;     /**< My public certificate signed by CA */
};

struct certBundle *generateCSR(char *cn) {
	if(cn == NULL)
		return NULL;
	RSA *rsa;
	X509_NAME *name = NULL;
	struct certBundle *bundle = (struct certBundle *)malloc(sizeof(struct certBundle));
	bundle->pkey = NULL;
	bundle->csr = NULL;
	bundle->cert = NULL;
	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

	if((bundle->pkey = EVP_PKEY_new()) == NULL) {
		certBundleFree(bundle);
		return NULL;
	}

	if((bundle->csr = X509_REQ_new()) == NULL) {
		certBundleFree(bundle);
		return NULL;
	}

	rsa = RSA_generate_key(1024, RSA_F4, NULL, NULL);

	if(!EVP_PKEY_assign_RSA(bundle->pkey, rsa)) {
		certBundleFree(bundle);
		return NULL;
	}

	rsa = NULL;

	X509_REQ_set_pubkey(bundle->csr, bundle->pkey);

	name = X509_REQ_get_subject_name(bundle->csr);

	X509_NAME_add_entry_by_txt(name,"C", MBSTRING_ASC, (unsigned char *)"IT", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name,"ST", MBSTRING_ASC, (unsigned char *)"Italy", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name,"L", MBSTRING_ASC, (unsigned char *)"Turin", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name,"O", MBSTRING_ASC, (unsigned char *)"Urmet s.p.a.", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name,"OU", MBSTRING_ASC, (unsigned char *)"Video Surveillance", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name,"CN", MBSTRING_ASC, (unsigned char *)cn, -1, -1, 0);

	if(!X509_REQ_sign(bundle->csr, bundle->pkey, EVP_md5())) {
		certBundleFree(bundle);
		return NULL;
	}
	return bundle;
}

//PRIVATE
IOTC_PRIVATE char *bioToString(BIO *bio) {
	long i;
	char *buf = NULL;
	long len = BIO_get_mem_data(bio, &buf);
	if(len < 0)
		return NULL;
	char *buffer = (char *)malloc(len+1);
	for(i=0; i<len; i++)
		buffer[i] = buf[i];
	buffer[len] = '\0';
	return buffer;
}

void printPkey(FILE *fd, struct certBundle *bundle) {
	if(bundle != NULL && bundle->pkey != NULL) {
		PEM_write_PrivateKey(fd, bundle->pkey, NULL, NULL, 0, NULL, NULL);
	}
}

char *pKeyToString(struct certBundle *bundle) {
	if(bundle != NULL && bundle->pkey != NULL) {
		BIO *bio = BIO_new(BIO_s_mem());
		if(bio != NULL && PEM_write_bio_PrivateKey(bio, bundle->pkey, NULL, NULL, 0, NULL, NULL)) {
			char *ret = bioToString(bio);
			BIO_free(bio);
			return ret;
		}
	}
	return NULL;
}

void printCsr(FILE *fd, struct certBundle *bundle) {
	if(bundle != NULL && bundle->csr != NULL) {
		PEM_write_X509_REQ(fd, bundle->csr);
	}
}

char *csrToString(struct certBundle *bundle) {
	if(bundle != NULL && bundle->csr != NULL) {
		BIO *bio = BIO_new(BIO_s_mem());
		if(bio != NULL && PEM_write_bio_X509_REQ(bio, bundle->csr)) {
			char *ret = bioToString(bio);
			BIO_free(bio);
			return ret;
		}
	}
	return NULL;
}

void printCert(FILE *fd, struct certBundle *bundle) {
	if(bundle != NULL && bundle->cert != NULL) {
		PEM_write_X509(fd, bundle->cert);
	}
}

char *certToString(struct certBundle *bundle) {
	if(bundle != NULL && bundle->cert != NULL) {
		BIO *bio = BIO_new(BIO_s_mem());
		if(bio != NULL && PEM_write_bio_X509(bio, bundle->cert))
			return bioToString(bio);
	}
	return NULL;
}

void certBundleFree(struct certBundle *bundle) {
	if(bundle != NULL) {
		if(bundle->csr != NULL)
			X509_REQ_free(bundle->csr);
		if(bundle->pkey != NULL)
			EVP_PKEY_free(bundle->pkey);
		if(bundle->cert != NULL)
			X509_free(bundle->cert);
	}
	free(bundle);
}

// PRIVATE
IOTC_PRIVATE int verify_server_cert_cb(int ok, X509_STORE_CTX *ctx) {
	return 1;
}

// PRIVATE
IOTC_PRIVATE void httpsCtxFree(struct httpsCtx *httpsCtx) {
	// close all connections
	if(httpsCtx->ssl != NULL) {
		SSL_shutdown(httpsCtx->ssl);
		SSL_free(httpsCtx->ssl);
		httpsCtx->ssl = NULL;
	}
	if(httpsCtx->sock >= 0) {
		close(httpsCtx->sock);
		httpsCtx->sock = -1;
	}
	if(httpsCtx->sslCtx != NULL) {
		SSL_CTX_free(httpsCtx->sslCtx);
		httpsCtx->sslCtx = NULL;
	}
	if(httpsCtx->gSocketClient != NULL) {
		g_object_unref(httpsCtx->gSocketClient);
		httpsCtx->gSocketClient = NULL;
	}
	if(httpsCtx->gSocketConnection != NULL) {
		g_object_unref(httpsCtx->gSocketConnection);
		httpsCtx->gSocketConnection = NULL;
	}
	if(httpsCtx->host != NULL)
		free(httpsCtx->host);
	if(httpsCtx->path != NULL)
		free(httpsCtx->path);
	if(httpsCtx->CAFile != NULL)
		free(httpsCtx->CAFile);
	if(httpsCtx->CAPath != NULL)
		free(httpsCtx->CAPath);
	if(httpsCtx->crtFile != NULL)
		free(httpsCtx->crtFile);
	if(httpsCtx->keyFile != NULL)
		free(httpsCtx->keyFile);
	if(httpsCtx->postMsg != NULL)
		free(httpsCtx->postMsg);
	free(httpsCtx);
}

// PRIVATE
IOTC_PRIVATE struct httpsCtx *httpsCtxNew(char *host, unsigned short port, char *path,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile, char *postMsg,
		void (*onResponse)(int, char *, void *), void *userData) {
	struct httpsCtx *httpsCtx = (struct httpsCtx *)malloc(sizeof(struct httpsCtx));
	httpsCtx->sock = -1;
	httpsCtx->ssl = NULL;
	httpsCtx->sslCtx = NULL;
	httpsCtx->gSocketClient = NULL;
	httpsCtx->gSocketConnection = NULL;
	httpsCtx->host = host != NULL ? strdup(host) : NULL;
	httpsCtx->port = port;
	httpsCtx->path = path != NULL ? strdup(path) : NULL;
	httpsCtx->CAFile = CAFile != NULL ? strdup(CAFile) : NULL;
	httpsCtx->CAPath = CAPath != NULL ? strdup(CAPath) : NULL;
	httpsCtx->crtFile = crtFile != NULL ? strdup(crtFile) : NULL;
	httpsCtx->keyFile = keyFile != NULL ? strdup(keyFile) : NULL;
	httpsCtx->postMsg = postMsg != NULL ? strdup(postMsg) : NULL;
	httpsCtx->callback = onResponse;
	httpsCtx->userData = userData;
	return httpsCtx;
}

// PRIVATE
IOTC_PRIVATE int httpsPrepareSend(struct httpsCtx *httpsCtx) {
	int error;
	char requestString[HTTP_MAX_RESP];

	if(httpsCtx->sock < 0) {
#ifdef DEBUG
		printf("Invalid socket. Did you create it?\n");
#endif
		return -1;
	}

	// initialize SSL context with ca cert and key
	SSLeay_add_ssl_algorithms();
	httpsCtx->sslCtx = SSL_CTX_new(TLSv1_client_method());
	if(httpsCtx->sslCtx == NULL) {
#ifdef DEBUG
		printf("Cannot intialize context\n");
#endif
		return -1;
	}
	SSL_CTX_set_verify(httpsCtx->sslCtx, SSL_VERIFY_PEER, verify_server_cert_cb);
	SSL_CTX_load_verify_locations(httpsCtx->sslCtx, httpsCtx->CAFile, httpsCtx->CAPath);
//#ifndef IOTC_CLIENT
	SSL_CTX_set_default_verify_paths(httpsCtx->sslCtx);
//#endif
//	SSL_CTX_use_certificate_chain_file(httpsCtx->sslCtx, crtFile);
	if(httpsCtx->crtFile != NULL && httpsCtx->keyFile != NULL) {
		SSL_CTX_use_certificate_file(httpsCtx->sslCtx, httpsCtx->crtFile, SSL_FILETYPE_PEM);
		SSL_CTX_use_PrivateKey_file(httpsCtx->sslCtx, httpsCtx->keyFile, SSL_FILETYPE_PEM);

		error = SSL_CTX_check_private_key(httpsCtx->sslCtx);
#ifdef DEBUG
		if(error != 1)
			printf("Private Key not verified\n");
#endif
	}

	// SSL connection
	httpsCtx->ssl = SSL_new(httpsCtx->sslCtx);
	if(httpsCtx->ssl == NULL) {
#ifdef DEBUG
		printf("Cannot intialize ssl\n");
#endif
		return -1;
	}
	SSL_set_fd(httpsCtx->ssl, httpsCtx->sock);

	while(true) {
		// repeat connect when SOCKET is non blocking and error is WANT_READ or WANT_WRITE
		error = SSL_connect(httpsCtx->ssl);
		if(error == -1) {
			int problem = SSL_get_error(httpsCtx->ssl, error);
			switch(problem) {
				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_WRITE:
					continue;
				break;
			}
#ifdef DEBUG
			printf("SSL connect failed\n");
#endif
			return -1;
		} else {
			break;
		}
	}

/*	if((error = SSL_get_verify_result(httpsCtx->ssl)) != X509_V_OK) {
#ifdef DEBUG
		printf("SSL cert verify failed [ %d ]\n", error);
#endif
		if(error != X509_V_ERR_CERT_NOT_YET_VALID)
			return -1;
	}
*/

#ifdef DEBUG
	// print certificate informations
	X509 *serverCert;
	char *crtStr;
	printf("SSL connection using %s\n", SSL_get_cipher(httpsCtx->ssl));
	serverCert = SSL_get_peer_certificate(httpsCtx->ssl);
	if(serverCert == NULL) {
		printf("Cannot get server cert\n");
		return -1;
	}
	printf ("Server certificate:\n");
	crtStr = X509_NAME_oneline(X509_get_subject_name(serverCert), 0, 0);
	if(crtStr != NULL) {
		printf("\t subject: %s\n", crtStr);
		OPENSSL_free(crtStr);
	}
	crtStr = X509_NAME_oneline(X509_get_issuer_name(serverCert), 0, 0);
	if(crtStr != NULL) {
		printf("\t issuer: %s\n", crtStr);
		OPENSSL_free(crtStr);
	}
	X509_free(serverCert);
#endif

	if(httpsCtx->postMsg == NULL) // Http GET
		snprintf(requestString, sizeof(requestString), "GET %s HTTP/1.1\nUser-Agent: IoTl/%s\nAccept: */*\nHost: %s\nConnection: Close\nContent-Length: 0\n\n", httpsCtx->path, VERSION, httpsCtx->host);
	else // Http POST
		snprintf(requestString, sizeof(requestString), "POST %s HTTP/1.1\nUser-Agent: IoTl/%s\nAccept: */*\nHost: %s\nConnection: Close\nContent-Type: application/x-www-form-urlencoded\nContent-Length: %ld\n\n%s", httpsCtx->path, VERSION, httpsCtx->host, strlen(httpsCtx->postMsg), httpsCtx->postMsg);
	error = SSL_write(httpsCtx->ssl, requestString, strlen(requestString));
	if(error == -1) {
#ifdef DEBUG
		printf("Cannot write to SSL socket\n");
#endif
		return -1;
	}

	return 0;
}

// PRIVATE
IOTC_PRIVATE int httpsReceive(struct httpsCtx *httpsCtx, char **response) {
	int error, offset;
	(*response) = (char *)malloc(sizeof(char)*HTTP_MAX_RESP);
	offset = 0;
	error = 1;
	while(error > 0) {
		error = SSL_read(httpsCtx->ssl, (*response)+offset, HTTP_MAX_RESP - (offset+1));
		switch(SSL_get_error(httpsCtx->ssl, error)) {
			case SSL_ERROR_NONE:
				offset += error;
				(*response)[offset] = '\0';
			break;
			case SSL_ERROR_WANT_READ:
				error = 1;
			break;
#ifdef DEBUG
			default:
				if(error == 0)
					printf("SSL no more data to read\n");
				else
					printf("SSL read error\n");
			break;
#endif
		}
	}
#ifdef DEBUG
	if(offset > 0)
		printf("Received %d bytes:\n%s\n", offset, *response);
#endif

	// read HTTP status code
	if(offset > 11 && strstr(*response, "HTTP/1.1") == (*response) && strlen(*response) > 11) {
#ifdef DEBUG
		printf("HTTP status code: %d\n", atoi((*response)+9));
#endif
		return atoi((*response)+9);
	} 
#ifdef DEBUG
	printf("Cannot read HTTP status code\n");
#endif
	return -1;
}

// PRIVATE
IOTC_PRIVATE int httpsConnectSocket(struct httpsCtx *httpsCtx, char *host, unsigned short port) {
	int error;
	struct addrinfo hints, *dnsResults, *dnsResult;

	// set connection hints
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	// Resolve host name
	error = getaddrinfo(host, NULL, &hints, &dnsResults);
	if(error != 0) {
#ifdef DEBUG
		printf("Cannot resolve host name\n");
#endif
		return -1;
	}

	// try to connect to one of the results gave by dns
	for(dnsResult = dnsResults; dnsResult != NULL; dnsResult = dnsResult->ai_next) {
		httpsCtx->sock = socket(dnsResult->ai_family, dnsResult->ai_socktype, dnsResult->ai_protocol);
		if(httpsCtx->sock == -1)
			continue;
		if(dnsResult->ai_family == PF_INET) {
			((struct sockaddr_in *)dnsResult->ai_addr)->sin_port = htons(port);
		} else if(dnsResult->ai_family == PF_INET6) {
			((struct sockaddr_in6 *)dnsResult->ai_addr)->sin6_port = htons(port);
		} else {
			continue;
		}
		if(connect(httpsCtx->sock, dnsResult->ai_addr, dnsResult->ai_addrlen) != -1)
			break;
		close(httpsCtx->sock);
	}
	if(dnsResult == NULL) {
#ifdef DEBUG
		printf("Cannot establish socket connection\n");
#endif
		freeaddrinfo(dnsResults);
		return -1;
	}
	freeaddrinfo(dnsResults);
	return httpsCtx->sock;
}

// PRIVATE
IOTC_PRIVATE int httpsSend(char *host, unsigned short port, char *path, char **response,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile, char *postMsg) {
	int error, code;
	(*response) = NULL;

	struct httpsCtx *httpsCtx = httpsCtxNew(host, port, path, CAFile, CAPath, crtFile, keyFile, postMsg,
			NULL, NULL);
	error = httpsConnectSocket(httpsCtx, host, port);
	if(error < 0) {
#ifdef DEBUG
		printf("[DEBUG] Cannot connect Socket\n");
#endif
		httpsCtxFree(httpsCtx);
		return -1;
	}
	error = httpsPrepareSend(httpsCtx);
	if(error < 0) {
#ifdef DEBUG
		printf("[DEBUG] Cannot prepare Https Send\n");
#endif
		httpsCtxFree(httpsCtx);
		return -1;
	}
	code = httpsReceive(httpsCtx, response);

	httpsCtxFree(httpsCtx);
	return code;
}

int httpsGet(char *host, unsigned short port, char *path, char **response,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile) {
	return httpsSend(host, port, path, response, CAFile, CAPath, crtFile, keyFile, NULL);
}

int httpsPost(char *host, unsigned short port, char *path, char **response,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile, char *postMsg) {
	return httpsSend(host, port, path, response, CAFile, CAPath, crtFile, keyFile, postMsg);
}

IOTC_PRIVATE gboolean httpsRecvCb(GObject *sourceObject, GAsyncResult *res, gpointer userData) {
	struct httpsCtx *httpsCtx = (struct httpsCtx *)userData;
	int code;
	char *response;

	code = httpsReceive(httpsCtx, &response);

	if(httpsCtx->callback != NULL)
		httpsCtx->callback(code, response, httpsCtx->userData);

	if(response != NULL)
		free(response);

	httpsCtxFree(httpsCtx);
	return G_SOURCE_REMOVE;
}

IOTC_PRIVATE void httpsSocketConnectCb(GObject *sourceObject, GAsyncResult *res, gpointer userData) {
	int error;
	struct httpsCtx *httpsCtx = (struct httpsCtx *)userData;
	httpsCtx->gSocketConnection = g_socket_client_connect_to_host_finish(httpsCtx->gSocketClient,
			res, NULL);
	GSocket *gsocket = g_socket_connection_get_socket(httpsCtx->gSocketConnection);
	httpsCtx->sock = g_socket_get_fd(gsocket);
	error = httpsPrepareSend(httpsCtx);
	if(error < 0) {
#ifdef DEBUG
		printf("[DEBUG] Cannot prepare Https Send\n");
#endif
		httpsCtx->callback(-1, NULL, httpsCtx->userData);
		httpsCtxFree(httpsCtx);
		return;
	}

	GIOChannel* channel = g_io_channel_unix_new(httpsCtx->sock);
	g_io_add_watch(channel, G_IO_IN, (GIOFunc)httpsRecvCb, httpsCtx);
	g_io_channel_unref(channel);
}

IOTC_PRIVATE int httpsSendAsync(char *host, unsigned short port, char *path,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile, char *postMsg,
		void (*onResponse)(int, char *, void *), void *userData) {
	struct httpsCtx *httpsCtx = httpsCtxNew(host, port, path, CAFile, CAPath, crtFile, keyFile, postMsg,
			onResponse, userData);
	httpsCtx->gSocketClient = g_socket_client_new();
	g_socket_client_connect_to_host_async(httpsCtx->gSocketClient, host, port, NULL,
			httpsSocketConnectCb, httpsCtx);
	return 0;
}

int httpsGetAsync(char *host, unsigned short port, char *path,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile,
		void (*onResponse)(int, char *, void *), void *userData) {
	return httpsSendAsync(host, port, path, CAFile, CAPath, crtFile, keyFile, NULL, onResponse, userData);
}

int httpsPostAsync(char *host, unsigned short port, char *path,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile, char *postMsg,
		void (*onResponse)(int, char *, void *), void *userData) {
	return httpsSendAsync(host, port, path, CAFile, CAPath, crtFile, keyFile, postMsg, onResponse, userData);
}

char *pemToUrl(char *pem) {
	int i;
	char *out;
	int offset = 0;
	for(i=0; i<strlen(pem); i++) {
		if(pem[i] == '\n' || pem[i] == '+' || pem[i] == '/' || pem[i] == ' ')
			offset++;
	}
	out = (char *)malloc(sizeof(char)*(strlen(pem)+(offset*2)+1));
	if(out != NULL) {
		offset = 0;
		for(i=0; i<=strlen(pem); i++) {
			if(pem[i] == '\n') {
				out[i+offset] = '%';
				offset++;
				out[i+offset] = '0';
				offset++;
				out[i+offset] = 'A';
			} else if(pem[i] == '+') {
				out[i+offset] = '%';
				offset++;
				out[i+offset] = '2';
				offset++;
				out[i+offset] = 'B';
			} else if(pem[i] == '/') {
				out[i+offset] = '%';
				offset++;
				out[i+offset] = '2';
				offset++;
				out[i+offset] = 'F';
			} else if(pem[i] == ' ') {
				out[i+offset] = '%';
				offset++;
				out[i+offset] = '2';
				offset++;
				out[i+offset] = '0';
			} else {
				out[i+offset] = pem[i];
			}
		}
	}
	return out;
}
//#else  // IOTC_CLIENT
#if 0
struct certBundle *generateCSR(char *cn) { return NULL; }

void printPkey(FILE *fd, struct certBundle *bundle) {}

char *pKeyToString(struct certBundle *bundle) { return NULL; }

void printCsr(FILE *fd, struct certBundle *bundle) {}

char *csrToString(struct certBundle *bundle) { return NULL; }

void printCert(FILE *fd, struct certBundle *bundle) {}

char *certToString(struct certBundle *bundle) { return NULL; }

void certBundleFree(struct certBundle *bundle) {}

int httpsGet(char *host, unsigned short port, char *path, char **response,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile) { return -1; }

int httpsGetAsync(char *host, unsigned short port, char *path,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile,
		void (*onResponse)(int, char *, void *), void *userData) { return -1; }

int httpsPost(char *host, unsigned short port, char *path, char **response,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile, char *postMsg) { return -1; }

int httpsPostAsync(char *host, unsigned short port, char *path,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile, char *postMsg,
		void (*onResponse)(int, char *, void *), void *userData) { return -1; }

char *pemToUrl(char *pem) { return NULL; }
#endif
//#endif  // IOTC_CLIENT
