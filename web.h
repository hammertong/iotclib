/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 *
 * [any text provided by the client]
 *
 */

/**
 * @file web.h
 * @author Matteo Di Leo <matteo.dileo@csp.it>
 * @date 18/09/2015
 * @brief Urmet IoT http and JSON decode implementation
 *
 * Here are placed all the functions used for http requests
 * and for parsing of cloud web API response
 */

#include "library.h"

/**
 * @brief The structure used to list connection servers (broker/stun/turn)
 *
 * @warn free() of the structure should be avoided, use instead iotcServerListDeleteFirst()
 * 		and iotcServerListFree()
 */
struct iotcServerList {
	char *ip;			/**< The ip (as string) of the server */
	char *username;			/**< Username used for turn authentication */
	char *password;			/**< Password used for turn authentication */
	struct iotcServerList *next;	/**< Next element of the list (or NULL if this is the last one) */
};

/**
 * @brief Get the list of connection servers
 *
 * This function retrieve the list of connection servers from main server via an HTTPs request.
 * This is a synchronous request that involve networking operations, so it may take some time.
 * @param CAFile The full path of CA certificate used to verify server identity.
 * @param CAPath The path of directory where is contained CA certificate. Only one between
 * 		CAFile and CAPath is strictly needed.
 * @param crtFile The full path of my certificate used by server to verify my identity
 * @param keyFile The full path of my private key used to crypt my messages
 */
struct iotcServerList *webDeviceGetServers(char *CAFile, char *CAPath, char *crtFile, char *keyFile);

/**
 * @brief Get the list of connection servers asynchronously
 *
 * This function retrieve the list of connection servers from main server via an HTTPs request.
 * This function returns immediatly and invoke a callback when server list is ready (callback is
 * invoked even if list is empty).
 * @see webDeviceGetServers()
 * @param CAFile The full path of CA certificate used to verify server identity.
 * @param CAPath The path of directory where is contained CA certificate. Only one between
 * 		CAFile and CAPath is strictly needed.
 * @param crtFile The full path of my certificate used by server to verify my identity
 * @param keyFile The full path of my private key used to crypt my messages
 * @param onFinish The callback invoked when list is ready
 * @param userData A pointer to data passed back to callback
 */
int webDeviceGetServersAsync(char *CAFile, char *CAPath, char *crtFile, char *keyFile,
		void (*onFinish)(struct iotcServerList *, void *), void *userData);

/**
 * @brief Notify the main server which connection server this device connected to
 *
 * @see webDeviceGetServers()
 * @see webDeviceGetServersAsync()
 * @param ip The ip of the connection server this device connected to (ip is one of the list
 * 		retrieved invoking webDeviceGetServers() or webDeviceGetServersAsync())
 * @param CAFile The full path of CA certificate used to verify server identity.
 * @param CAPath The path of directory where is contained CA certificate. Only one between
 * 		CAFile and CAPath is strictly needed.
 * @param crtFile The full path of my certificate used by server to verify my identity
 * @param keyFile The full path of my private key used to crypt my messages
 */
bool webDeviceRegister(char *ip, char *CAFile, char *CAPath, char *crtFile, char *keyFile);

/**
 * @brief Send an http request using POST method
 *
 * @param host The hostname of the server to connect to
 * @param path The path of HTTP request
 * @param[out] response The HTTP response
 * @param postMsg Data to be sent into the request
 * @return The HTTP status code returned by server, or a negative number if an error occurred
 */
int httpPost(char *host, char *path, char **response, char *postMsg);

/**
 * @brief Sena an http request using POST method and invoke callback when response is ready
 *
 * @param host The hostname of the server to connect to
 * @param port The port of the server to connect to
 * @param path The path of HTTP request
 * @param postMsg Data to be sent into the request
 * @param onResponse The callback invoked when data is ready if the function returns 0. Params are:
 *	- code The HTTP status code
 *	- response The string containing http response
 *	- userData the user data provided as parameter in this funtion
 * @param userData A pointer to data passed back to callbacks
 * @return A negative error code or 0 if ok
 */
int httpPostAsync(char *host, unsigned short port, char *path, char *postMsg,
		void (*onResponse)(int, char *, void *), void *userData);

/**
 * @brief Remove and deallocate first element of the server list
 *
 * @param list The address of a list pointer
 */
void iotcServerListDeleteFirst(struct iotcServerList **list);

/**
 * @brief Deallocate server list
 *
 * @param list The list to be freed
 */
void iotcServerListFree(struct iotcServerList *list);
