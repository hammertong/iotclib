/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 *
 * [any text provided by the client]
 *
 * http.c
 *	Urmet IoT http and JSON decode implementation
 *
 * Authors:
 *	Matteo Di Leo <matteo.dileo@csp.it>
 */

#include "web.h"
#include "secure.h"
#include <unistd.h>
#include <errno.h>
#include <glib/glib.h>
#include <gio/gio.h>

struct httpCtx {
	int sock;
	GSocketClient *gSocketClient;
	GSocketConnection *gSocketConnection;
	char *host;
	unsigned short port;
	char *path;
	char *postMsg;
	void (*callback)(int, char *, void*);
	void *userData;
};

struct webCtx {
	void *callback;
	void *userData;
};

IOTC_PRIVATE char *getJSONVal(char *jsonObject, char *field) {
	if(field == NULL)
		return NULL;
	char completeField[strlen(field)+5];
	char *valueStart, *valueEnd;
	char *ret;
	snprintf(completeField, strlen(field)+5, "\"%s\":\"", field);
	valueStart = strstr(jsonObject, completeField) + strlen(completeField);
	if(valueStart == NULL)
		return NULL;
	valueEnd = strstr(valueStart, "\"");
	if(valueEnd == NULL)
		return NULL;
	valueEnd[0] = '\0';
	ret = strdup(valueStart);
	valueEnd[0] = '\"';
	return ret;
}

IOTC_PRIVATE struct iotcServerList *parseList(char *response) {
	struct iotcServerList *list, *last;
	char *json, *objEnd;
	char *ip, *user, *pass;
	list = NULL;
	last = NULL;

	json = strstr(response, "\n\n\[");
	if(json == NULL) {
#ifdef DEBUG
		printf("[DEBUG] Cannot parse JSON!\n");
#endif
		return NULL;
	}

	json += 3;
	while(json < response+(strlen(response))) {
		if((objEnd = strstr(json, "}")) == NULL) {
#ifdef DEBUG
			printf("[DEBUG] Wrong JSON format: missing }\n");
#endif
			break;
		}
		if((objEnd+1) >= response+(strlen(response))) {
#ifdef DEBUG
			printf("[DEBUG] Wrong JSON\n");
#endif
			break;
		}
		objEnd[1] = '\0';

		ip = getJSONVal(json, "srv_ip");
		user = getJSONVal(json, "srv_username");
		pass = getJSONVal(json, "srv_psw");
		if(ip == NULL || user == NULL || pass == NULL) {
			if(ip!=NULL) free(ip);
			if(user!=NULL) free(user);
			if(pass!=NULL) free(pass);
			break;
		}

		struct iotcServerList *server = (struct iotcServerList *)malloc(sizeof(struct iotcServerList));
		server->ip = ip;
		server->username = user;
		server->password = pass;
		server->next = NULL;

		if(list == NULL) {
			list = server;
			last = list;
		} else {
			last->next = server;
			last = last->next;
		}

		objEnd[1] = ',';
		json = objEnd+2;
	}
	return list;
}

struct iotcServerList *webDeviceGetServers(char *CAFile, char *CAPath, char *crtFile, char *keyFile) {
	struct iotcServerList *list;
	char *response;

	int code = httpsGet(SERVER_NAME, 4343, "/devicegetservers/", &response,
			CAFile, CAPath, crtFile, keyFile);

	if(code != 200) {
#ifdef DEBUG
		if(code > 0)
			printf("webDeviceGetServers ERROR\n%s\n", response);
		else
			printf("webDeviceGetServers ERROR\n");
#endif
		if(response != NULL)
			free(response);
		return NULL;
	}
	list = parseList(response);
	free(response);
	return list;
}

IOTC_PRIVATE void webOnDeviceGetServersHttpsResponse(int code, char *response, void *userData) {
	struct webCtx *webCtx = (struct webCtx *)userData;
#ifdef DEBUG
	printf("[DEBUG] Web servers response HTTP[%d]\n", code);
#endif
	if(code != 200) {
#ifdef DEBUG
		if(code > 0)
			printf("webDeviceGetServers ERROR\n%s\n", response);
		else
			printf("webDeviceGetServers ERROR\n");
#endif
		((void (*)(struct iotcServerList *, void *))webCtx->callback)(NULL, webCtx->userData);
	} else {
		struct iotcServerList *list = parseList(response);
		((void (*)(struct iotcServerList *, void *))webCtx->callback)(list, webCtx->userData);
	}
	free(webCtx);
}

int webDeviceGetServersAsync(char *CAFile, char *CAPath, char *crtFile, char *keyFile,
		void (*onFinish)(struct iotcServerList *, void *), void *userData) {
	struct webCtx *webCtx = (struct webCtx *)malloc(sizeof(struct webCtx));
	webCtx->callback = onFinish;
	webCtx->userData = userData;
	int ret = httpsGetAsync(SERVER_NAME, 4343, "/devicegetservers/",
			CAFile, CAPath, crtFile, keyFile,
			webOnDeviceGetServersHttpsResponse, webCtx);
	if(ret != 0)
		free(webCtx);
	return ret;
}

bool webDeviceRegister(char *ip, char *CAFile, char *CAPath, char *crtFile, char *keyFile) {
	char *response;
	char *postMsg = (char *)malloc(strlen(ip)+5);
	sprintf(postMsg, "srv=%s", ip);
	int code = httpsPost(SERVER_NAME, 4343, "/deviceregister/", &response,
			CAFile, CAPath, crtFile, keyFile, postMsg);
	free(postMsg);
	if(code != 200) {
#ifdef DEBUG
		if(code > 0)
			printf("webDeviceRegister ERROR\n%s\n", response);
		else
			printf("webDeviceRegister ERROR\n");
#endif
		if(response != NULL)
			free(response);
		return false;
	}
	free(response);
	return code == 200;
}

IOTC_PRIVATE void httpCtxFree(struct httpCtx *httpCtx) {
	// close all connections
	if(httpCtx->sock >= 0) {
		close(httpCtx->sock);
		httpCtx->sock = -1;
	}
	if(httpCtx->gSocketClient != NULL) {
		g_object_unref(httpCtx->gSocketClient);
		httpCtx->gSocketClient = NULL;
	}
	if(httpCtx->gSocketConnection != NULL) {
		g_object_unref(httpCtx->gSocketConnection);
		httpCtx->gSocketConnection = NULL;
	}
	if(httpCtx->host != NULL)
		free(httpCtx->host);
	if(httpCtx->path != NULL)
		free(httpCtx->path);
	if(httpCtx->postMsg != NULL)
		free(httpCtx->postMsg);
	free(httpCtx);
}

IOTC_PRIVATE struct httpCtx *httpCtxNew(char *host, unsigned short port, char *path, char *postMsg,
		void (*onResponse)(int, char *, void *), void *userData) {
	struct httpCtx *httpCtx = (struct httpCtx *)malloc(sizeof(struct httpCtx));
	httpCtx->sock = -1;
	httpCtx->gSocketClient = NULL;
	httpCtx->gSocketConnection = NULL;
	httpCtx->host = host != NULL ? strdup(host) : NULL;
	httpCtx->port = port;
	httpCtx->path = path != NULL ? strdup(path) : NULL;
	httpCtx->postMsg = postMsg != NULL ? strdup(postMsg) : NULL;
	httpCtx->callback = onResponse;
	httpCtx->userData = userData;
	return httpCtx;
}

IOTC_PRIVATE int httpPrepareSend(struct httpCtx *httpCtx) {
	int sent;
	char requestString[HTTP_MAX_RESP];
	if(httpCtx->sock < 0) {
#ifdef DEBUG
		printf("[DEBUG] Socket not connected\n");
#endif
		return -1;
	}
	// send request
	snprintf(requestString, sizeof(requestString), "POST %s HTTP/1.1\nUser-Agent: IoTl/%s\nAccept: */*\nHost: %s\nConnection: Close\nContent-Type: application/x-www-form-urlencoded\nContent-Length: %d\n\n%s", httpCtx->path, VERSION, httpCtx->host, strlen(httpCtx->postMsg), httpCtx->postMsg);
	sent = send(httpCtx->sock, requestString, strlen(requestString), 0);
	if(sent < strlen(requestString))
		return -1;
	return 0;
}

IOTC_PRIVATE int httpReceive(struct httpCtx *httpCtx, char **response) {
	int error, offset;
	(*response) = (char *)malloc(sizeof(char)*HTTP_MAX_RESP);
	offset = 0;
	error = 1;
	while(error > 0) {
		error = recv(httpCtx->sock, (*response)+offset, HTTP_MAX_RESP - (offset+1), 0);
		if(error == 0 || (error == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) {
			break;
		} else if(error == -1) {
			error = 1;
		} else {
			offset += error;
		}
	}
#ifdef DEBUG
	if(offset > 0)
		printf("Received %d bytes:\n%s\n", offset, *response);
#endif

	if(strstr(*response, "HTTP/1.1") == (*response) && strlen(*response) > 11) {
#ifdef DEBUG
		printf("HTTP status code: %d\n", atoi((*response)+9));
#endif
		return atoi((*response)+9);
	}
	return -1;
}

IOTC_PRIVATE gboolean httpRecvCb(GObject *sourceObject, GAsyncResult *res, gpointer userData) {
	struct httpCtx *httpCtx = (struct httpCtx *)userData;
	int code;
	char *response;

	code = httpReceive(httpCtx, &response);

	if(httpCtx->callback != NULL)
		httpCtx->callback(code, response, httpCtx->userData);

	if(response != NULL)
		free(response);

	httpCtxFree(httpCtx);
	return G_SOURCE_REMOVE;
}

IOTC_PRIVATE void httpSocketConnectCb(GObject *sourceObject, GAsyncResult *res, gpointer userData) {
	int error;
	struct httpCtx *httpCtx = (struct httpCtx *)userData;
	httpCtx->gSocketConnection = g_socket_client_connect_to_host_finish(httpCtx->gSocketClient,
			res, NULL);
	GSocket *gsocket = g_socket_connection_get_socket(httpCtx->gSocketConnection);
	httpCtx->sock = g_socket_get_fd(gsocket);

	error = httpPrepareSend(httpCtx);
	if(error != 0) {
#ifdef DEBUG
		printf("[DEBUG] Cannot prepare Http Send\n");
#endif
		httpCtx->callback(-1, NULL, httpCtx->userData);
		httpCtxFree(httpCtx);
		return;
	}

	GIOChannel* channel = g_io_channel_unix_new(httpCtx->sock);
	g_io_add_watch(channel, G_IO_IN, (GIOFunc)httpRecvCb, httpCtx);
	g_io_channel_unref(channel);
}

IOTC_PRIVATE int httpSendAsync(char *host, unsigned short port, char *path, char *postMsg,
		void (*onResponse)(int, char *, void *), void *userData) {
	struct httpCtx *httpCtx = httpCtxNew(host, port, path, postMsg, onResponse, userData);
	httpCtx->gSocketClient = g_socket_client_new();
	g_socket_client_connect_to_host_async(httpCtx->gSocketClient, host, port, NULL,
			httpSocketConnectCb, httpCtx);
	return 0;
}

int httpPostAsync(char *host, unsigned short port, char *path, char *postMsg,
		void (*onResponse)(int, char *, void *), void *userData) {
	return httpSendAsync(host, port, path, postMsg, onResponse, userData);
}

int httpPost(char *host, char *path, char **response, char *postMsg) {
	// TODO some code can be reduced into a function to avoid duplication
	// between this file and secure.c implementation of https post
	int error, sock, offset;
	struct addrinfo hints, *dnsResults, *dnsResult;
	char requestString[HTTP_MAX_RESP];

	(*response) = NULL;

	// set connection hints
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	// Resolv host name
	error = getaddrinfo(host, NULL, &hints, &dnsResults);
	if(error != 0) {
#ifdef DEBUG
		printf("Cannot resolve host name\n");
#endif
		return -1;
	}

	// try to connect to one of the results gave by dns
	for(dnsResult = dnsResults; dnsResult != NULL; dnsResult = dnsResult->ai_next) {
		sock = socket(dnsResult->ai_family, dnsResult->ai_socktype, dnsResult->ai_protocol);
		if(sock == -1)
			continue;
		if(dnsResult->ai_family == PF_INET){
			((struct sockaddr_in *)dnsResult->ai_addr)->sin_port = htons(80);
		}else if(dnsResult->ai_family == PF_INET6){
			((struct sockaddr_in6 *)dnsResult->ai_addr)->sin6_port = htons(80);
		}else{
			continue;
		}
		if(connect(sock, dnsResult->ai_addr, dnsResult->ai_addrlen) != -1)
			break;
		close(sock);
	}
	if(dnsResult == NULL) {
#ifdef DEBUG
		printf("Cannot establish socket connection\n");
#endif
		freeaddrinfo(dnsResults);
		return -1;
	}
	freeaddrinfo(dnsResults);

	snprintf(requestString, sizeof(requestString), "POST %s HTTP/1.1\nUser-Agent: IoTl/%s\nAccept: */*\nHost: %s\nConnection: Close\nContent-Type: application/x-www-form-urlencoded\nContent-Length: %d\n\n%s", path, VERSION, host, strlen(postMsg), postMsg);

	send(sock, requestString, strlen(requestString), 0);

	// read response
	(*response) = (char *)malloc(sizeof(char)*HTTP_MAX_RESP);
	offset = 0;
	error = 1;
	while(error > 0) {
		error = recv(sock, (*response)+offset, HTTP_MAX_RESP - (offset+1), 0);
		if(error == 0 || (error == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) {
			break;
		} else if(error == -1) {
			error = 1;
		} else {
			offset += error;
		}
	}
#ifdef DEBUG
	if(offset > 0)
		printf("Received %d bytes:\n%s\n", offset, *response);
#endif
	close(sock);

	if(strstr(*response, "HTTP/1.1") == (*response) && strlen(*response) > 11) {
#ifdef DEBUG
		printf("HTTP status code: %d\n", atoi((*response)+9));
#endif
		return atoi((*response)+9);
	}
	return -1;
}

void iotcServerListDeleteFirst(struct iotcServerList **list) {
	if((*list) != NULL) {
		struct iotcServerList *tmp = (*list);
		(*list) = (*list)->next;
		free(tmp->ip);
		free(tmp->username);
		free(tmp->password);
		free(tmp);
	}
}

void iotcServerListFree(struct iotcServerList *list) {
	struct iotcServerList *sl = list;
	while(sl != NULL) {
		iotcServerListDeleteFirst(&sl);
	}
}
