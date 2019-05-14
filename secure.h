/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 * 
 * [any text provided by the client]
 * 
 */

/**
 * @file secure.h
 * @author Matteo Di Leo <matteo.dileo@csp.it>
 * @date 11/02/2015
 * @brief Urmet IoT ssl/tls and cert management implementation
 *
 * The Urmet IoT ssl/tls and cert management implementation.
 * Here are placed all the functions used for read/write/generate certificates.
 * Here are placed all the functions used for generic https connection with
 * client and server authentication by certificate.
 */

#ifndef __SECURE_H__
#define __SECURE_H__

#include "library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_MAX_RESP 8192

/**
 * @brief Generate csr
 *
 * Generate a private key and a Certificate Signing Request.
 * This function set pkey and csr fields (of certBundle) only, cert field is not modified
 * @see certBundle
 * @param cn The common name to be used for certificate
 * @return a certBundle structure with private key, csr and all informations needed to request a certificate
 *	or null if some error occurred
 */
struct certBundle *generateCSR(char *cn);

/**
 * @brief Print private key
 *
 * @param fd A descriptor of file or stream to dirige output
 * @param bundle A pointer to the structure which contains all certs and key
 */
void printPkey(FILE *fd, struct certBundle *bundle);

/**
 * @brief Get private key as a string
 *
 * Get the private key contained in certBundle (if any) as a string in PEM format
 * @param bundle A pointer to the structure which contains all certs and key
 * @return The key as string, or NULL if empty
 */
char *pKeyToString(struct certBundle *bundle);

/**
 * @brief Print Certificate Signing Request
 *
 * @param fd A descriptor of file or stream to dirige output
 * @param bundle A pointer to the structure which contains all certs and key
 */
void printCsr(FILE *fd, struct certBundle *bundle);

/**
 * @brief Get CSR as a string
 *
 * Get the Certificate Signing Request contained in certBundle (if any) as a string in PEM format
 * @param bundle A pointer to the structure which contains all certs and key
 * @return The csr as string, or NULL if empty
 */
char *csrToString(struct certBundle *bundle);

/**
 * @brief Print my public certificate signed by CA
 *
 * @param fd A descriptor of file or stream to dirige output
 * @param bundle A pointer to the structure which contains all certs and key
 */
void printCert(FILE *fd, struct certBundle *bundle);

/**
 * @brief Get client certificate as a string
 *
 * Get the client Certificate contained in certBundle (if any) as a string in PEM format
 * @param bundle A pointer to the structure which contains all certs and key
 * @return The cert as string, or NULL if empty
 */
char *certToString(struct certBundle *bundle);

/**
 * @brief Deallocate certBundle
 *
 * Deallocate certBundle structure to avoid memory leaks.
 * @param bundle A pointer to the structure which contains all certs and key
 * @see certBundle
 * @warning all not NULL fields are deallocated, be careful and avoid segfaults
 */
void certBundleFree(struct certBundle *bundle);

/**
 * @brief Send an HTTPs GET
 *
 * Send an HTTPs GET authenticating server using CA public certificate
 * and authenticating client with its public cert and crypting with private key
 * @param host The URL of the server (ex.: www.cloud.urmet.com)
 * @param port The port of the server to connect to (usually 443)
 * @param path The path of the resource (ex.: /devapi/auth.php)
 * @param[out] response The response of the server as string. Can be NULL if
 *	HTTP status code is != 200. Caller should deallocate variable when it is needed no more
 * @param CAFile The full path of CA certificate used to verify server identity.
 * @param CAPath The path of directory where is contained CA certificate. Only one between
 *	CAFile and CAPath is strictly needed. Prefer CAFile, because CAPath doesn't always work
 * @param crtFile The full path of my certificate used by server to verify my identity
 * @param keyFile The full path of my private key used to crypt my messages
 * @return The HTTP status code
 */
int httpsGet(char *host, unsigned short port, char *path, char **response,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile);

/**
 * @brief Send an HTTPs GET asynchronously
 *
 * Send an HTTPs GET authenticating server using CA public certificate
 * and authenticating client with its public cert and crypting with private key.
 * The function returns immediatly and invoke callback when data is ready.
 * @param host The URL of the server (ex.: www.cloud.urmet.com)
 * @param port The port of the server to connect to (usually 443)
 * @param path The path of the resource (ex.: /devapi/auth.php)
 * @param CAFile The full path of CA certificate used to verify server identity.
 * @param CAPath The path of directory where is contained CA certificate. Only one between
 *	CAFile and CAPath is strictly needed. Prefer CAFile, because CAPath doesn't always work
 * @param crtFile The full path of my certificate used by server to verify my identity
 * @param keyFile The full path of my private key used to crypt my messages
 * @param onResponse The callback invoked when data is ready if the function returns 0. Params are:
 *	- code The HTTP status code
 *	- response The string containing http response
 *	- userData the user data provided as parameter in this funtion
 * @param userData A pointer to data passed back to callbacks
 * @return A negative error code or 0 if ok
 */
int httpsGetAsync(char *host, unsigned short port, char *path,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile,
		void (*onResponse)(int, char *, void *), void *userData);

/**
 * @brief Send an HTTPs POST
 *
 * Send an HTTPs POST authenticating server using CA public certificate
 * and authenticating client with its public cert and crypting with private key
 * @param host The URL of the server (ex.: www.cloud.urmet.com)
 * @param port The port of the server to connect to (usually 443)
 * @param path The path of the resource (ex.: /devapi/auth.php)
 * @param[out] response The response of the server as string. Can be NULL if
 *      HTTP status code is != 200. Caller should deallocate variable when it is needed no more
 * @param CAFile The full path of CA certificate used to verify server identity.
 * @param CAPath The path of directory where is contained CA certificate. Only one between
 *      CAFile and CAPath is strictly needed. Prefer CAFile, because CAPath doesn't always work
 * @param crtFile The full path of my certificate used by server to verify my identity
 * @param keyFile The full path of my private key used to crypt my messages
 * @param postMsg The message to send into POST, must be URL Encoded
 * @return The HTTP status code
 */
int httpsPost(char *host, unsigned short port, char *path, char **response,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile, char *postMsg);

/**
 * @brief Send an HTTPs POST asynchronously
 *
 * Send an HTTPs POST authenticating server using CA public certificate
 * and authenticating client with its public cert and crypting with private key
 * The function returns immediatly and invoke callback when data is ready.
 * @param host The URL of the server (ex.: www.cloud.urmet.com)
 * @param port The port of the server to connect to (usually 443)
 * @param path The path of the resource (ex.: /devapi/auth.php)
 * @param CAFile The full path of CA certificate used to verify server identity.
 * @param CAPath The path of directory where is contained CA certificate. Only one between
 *      CAFile and CAPath is strictly needed. Prefer CAFile, because CAPath doesn't always work
 * @param crtFile The full path of my certificate used by server to verify my identity
 * @param keyFile The full path of my private key used to crypt my messages
 * @param postMsg The message to send into POST, must be URL Encoded
 * @param onResponse The callback invoked when data is ready if the function returns 0. Params are:
 *	- code The HTTP status code
 *	- response The string containing http response
 *	- userData the user data provided as parameter in this funtion
 * @param userData A pointer to data passed back to callbacks
 * @return A negative error code or 0 if ok
 */
int httpsPostAsync(char *host, unsigned short port, char *path,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile, char *postMsg,
		void (*onResponse)(int, char *, void *), void *userData);

/**
 * @brief URL Encode a PEM
 *
 * URL encode a certificate/cert request/key in PEM format to be sent over HTTP
 * @param pem The string containing PEM
 * @return A string containing URL encoded cert/key/csr
 */
char *pemToUrl(char *pem);

#endif /* __SECURE_H__ */
