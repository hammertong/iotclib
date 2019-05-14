/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 * 
 * [any text provided by the client]
 *
 * mqtt.c
 *      Urmet IoT mqtt signalling implementation
 *
 * Authors:
 *      Matteo Di Leo <matteo.dileo@csp.it>
 */

//#ifndef IOTC_CLIENT

#include "library.h"
#include "mqtt.h"
#include <glib/glib.h>

#define MQTT_PING_TIMEOUT 15
#define MQTT_DEAD_TIMEOUT 30

struct mqttCtx {
	struct mosquitto *mosq;
	void (*connectCb)(MqttCtx *, void *, int);
	void (*subscribeCb)(MqttCtx *, void *, int, int, const int *);
	void (*messageCb)(MqttCtx *, void *, const struct mosquitto_message *);
	void (*disconnectCb)(MqttCtx *, void *, int);
	void *userData;
	gulong mqttTimeout;
};

#ifdef DEBUG
// PRIVATE
IOTC_PRIVATE void privMqttLogCb(struct mosquitto *mosq, void *obj, int level, const char *str) {
	char *levelStr;
	switch(level) {
		case MOSQ_LOG_INFO:
			levelStr = "INFO";
		break;
		case MOSQ_LOG_NOTICE:
			levelStr = "NOTICE";
		break;
		case MOSQ_LOG_WARNING:
			levelStr = "WARNING";
		break;
		case MOSQ_LOG_ERR:
			levelStr = "ERROR";
		break;
		default:
			levelStr = "DEBUG";
		break;
	}
	printf("[%s] %s\n", levelStr, str);
}
#endif

IOTC_PRIVATE void privMqttDisconnectCb(struct mosquitto *mosq, void *obj, int rc) {
	MqttCtx *mqttCtx = (MqttCtx *)obj;
	void (*callback)(MqttCtx *, void *, int) = mqttCtx->disconnectCb;
	mqttCtx->disconnectCb = NULL;
#ifdef DEBUG
	printf("\033[31m[DEBUG] Disconnected [%d]\033[0m\n", rc);
	if(rc == MOSQ_ERR_ERRNO)
		printf("\033[31m[DEBUG] Mosquitto disconnected: errno - %s\033[0m\n", strerror(errno));
#endif
	if(callback != NULL)
		callback(mqttCtx, mqttCtx->userData, -1);
}

IOTC_PRIVATE void privMqttMessageCb(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
#ifdef DEBUG
	printf("[DEBUG] Message: %s\n", (char *)(message->payload));
#endif
	MqttCtx *mqttCtx = (MqttCtx *)obj;
	if(mqttCtx->messageCb != NULL)
		mqttCtx->messageCb(mqttCtx, mqttCtx->userData, message);
}

IOTC_PRIVATE void privMqttSubscribeCb(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos) {
#ifdef DEBUG
	printf("[DEBUG] Subscribed (mid: %d): %d\n", mid, granted_qos[0]);
#endif
	MqttCtx *mqttCtx = (MqttCtx *)obj;
	if(mqttCtx->mqttTimeout != 0) {
		g_source_remove(mqttCtx->mqttTimeout);
		mqttCtx->mqttTimeout = 0;
	}
	if(mqttCtx->subscribeCb != NULL)
		mqttCtx->subscribeCb(mqttCtx, mqttCtx->userData, mid, qos_count, granted_qos);
}

IOTC_PRIVATE void privMqttConnectCb(struct mosquitto *mosq, void *obj, int result) {
	MqttCtx *mqttCtx = (MqttCtx *)obj;
#ifdef DEBUG
	if(!result) {
		printf("[DEBUG] Connect ok\n");
	} else {
		printf("[DEBUG] Connect fail\n");
	}
#endif
	if(mqttCtx->connectCb != NULL)
		mqttCtx->connectCb(mqttCtx, mqttCtx->userData, result);
}

IOTC_PRIVATE gboolean privMqttTimeout(gpointer userData) {
#ifdef DEBUG
	printf("[DEBUG] Mqtt timeout expired\n");
#endif
	MqttCtx *mqttCtx = userData;
/*	if(mqttCtx->mosq != NULL) {
		mosquitto_disconnect(mqttCtx->mosq);
		mosquitto_loop_stop(mqttCtx->mosq, true);
	}*/
//	mqttCtx->mqttTimeout = 0;
	void (*callback)(MqttCtx *, void *, int) = mqttCtx->disconnectCb;
	mqttCtx->disconnectCb = NULL;
	if(callback != NULL)
		callback(mqttCtx, mqttCtx->userData, -1);
	return G_SOURCE_REMOVE;
}

MqttCtx *mqttNew(char *deviceId, char *host, int port,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile,
		void (*connectCb)(MqttCtx *, void *, int),
		void (*subscribeCb)(MqttCtx *, void *, int, int, const int *),
		void (*messageCb)(MqttCtx *, void *, const struct mosquitto_message *),
		void (*disconnectCb)(MqttCtx *, void *, int),
		void *userData) {
	char TESThost[INET6_ADDRSTRLEN];
	int error;
	// User agent name
	char *id = malloc(MOSQ_MQTT_ID_MAX_LENGTH); // MAX_LENGTH is 23
	snprintf(id, MOSQ_MQTT_ID_MAX_LENGTH, "%s", deviceId);

	// Setup context
	MqttCtx *mqttCtx = (MqttCtx *)malloc(sizeof(MqttCtx));
	mqttCtx->mosq = NULL;
	mqttCtx->connectCb = connectCb;
	mqttCtx->subscribeCb = subscribeCb;
	mqttCtx->messageCb = messageCb;
	mqttCtx->disconnectCb = disconnectCb;
	mqttCtx->userData = userData;
	mqttCtx->mqttTimeout = 0;

	// Create mosquitto instance with clean sesssion and no user data to callbacks
	mosquitto_lib_init();
	mqttCtx->mosq = mosquitto_new(id, true, mqttCtx);
	free(id);
	if(!mqttCtx->mosq) {
#ifdef DEBUG
		switch(errno) {
			case ENOMEM:
				printf("Cannot init mosquitto: Out of memory\n");
			break;
			case EINVAL:
				printf("Cannot init mosquitto: Invalid id and/or clean_session.\n");
			break;
			default:
				printf("Unknown error on mosquitto new\n");
			break;
		}
#endif
		mqttFree(mqttCtx);
		return NULL;
	}
#ifdef DEBUG
	mosquitto_log_callback_set(mqttCtx->mosq, privMqttLogCb);
#endif

	// Set TLS options: key and certificates, verify server, use TLSv1
	if((error = mosquitto_tls_set(mqttCtx->mosq, CAFile, CAPath, crtFile, keyFile, NULL))) {
#ifdef DEBUG
		switch(error) {
			case MOSQ_ERR_SUCCESS:
				printf("Mosquitto TLS set: success\n");
			break;
			case MOSQ_ERR_INVAL:
				printf("Mosquitto TLS set: invalid parameters\n");
			break;
			case MOSQ_ERR_NOMEM:
				printf("Mosquitto TLS set: Out of memory\n");
			break;
			default:
				printf("Mosquitto TLS set: Unknown error\n");
			break;
		}
#endif
		mqttFree(mqttCtx);
		return NULL;
	}
	if((error = mosquitto_tls_opts_set(mqttCtx->mosq, SSL_VERIFY_NONE, "tlsv1", NULL))) {
#ifdef DEBUG
		switch(error) {
			case MOSQ_ERR_SUCCESS:
				printf("Mosquitto TLS opts set: success\n");
			break;
			case MOSQ_ERR_INVAL:
				printf("Mosquitto TLS opts set: invalid parameters\n");
			break;
			case MOSQ_ERR_NOMEM:
				printf("Mosquitto TLS opts set: Out of memory\n");
			break;
			default:
				printf("Mosquitto TLS opts set: Unknown error\n");
			break;
		}
#endif
		mqttFree(mqttCtx);
		return NULL;
	}
	// Insecure means that server name can be different from the CN contained in certificate
	// it is false by default
	mosquitto_tls_insecure_set(mqttCtx->mosq, true);

	// Set callbacks
	mosquitto_connect_callback_set(mqttCtx->mosq, privMqttConnectCb);
	mosquitto_subscribe_callback_set(mqttCtx->mosq, privMqttSubscribeCb);
	mosquitto_message_callback_set(mqttCtx->mosq, privMqttMessageCb);
	mosquitto_disconnect_callback_set(mqttCtx->mosq, privMqttDisconnectCb);

// TEST use getaddrinfo to translate hostname: on iOS ipv4 is translated to ipv6 automatically
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
	}

	// try to connect to one of the results gave by dns
	for(dnsResult = dnsResults; dnsResult != NULL; dnsResult = dnsResult->ai_next) {
		if(dnsResult->ai_family == PF_INET) {
//			((struct sockaddr_in *)dnsResult->ai_addr)->sin_port = htons(port);
			inet_ntop(AF_INET, &(((struct sockaddr_in *)dnsResult->ai_addr)->sin_addr), TESThost, INET6_ADDRSTRLEN);
			break;
		} else if(dnsResult->ai_family == PF_INET6) {
//			((struct sockaddr_in6 *)dnsResult->ai_addr)->sin6_port = htons(port);
			inet_ntop(AF_INET6, &(((struct sockaddr_in *)dnsResult->ai_addr)->sin_addr), TESThost, INET6_ADDRSTRLEN);
			break;
		}
		// else try next if exists
	}
	freeaddrinfo(dnsResults);
// TEST

	// Connect to broker
	if((error = mosquitto_connect_async(mqttCtx->mosq, TESThost, port, MQTT_PING_TIMEOUT))) {
#ifdef DEBUG
		switch(error) {
			case MOSQ_ERR_SUCCESS:
				printf("Mosquitto connect: success\n");
			break;
			case MOSQ_ERR_INVAL:
				printf("Mosquitto connect: invalid parameters\n");
			break;
			case MOSQ_ERR_NOMEM:
				printf("Mosquitto connect: Out of memory\n");
			break;
			case MOSQ_ERR_ERRNO:
				printf("Mosquitto connect: errno - %s\n", strerror(errno));
			break;
			default:
				printf("Mosquitto connect: unknown error[%d]\n", error);
			break;
		}
#endif
		mqttFree(mqttCtx);
		return NULL;
	}
#ifdef DEBUG
	printf("[DEBUG] MQTT CONNECT resp: %s\n", mosquitto_connack_string(error));
#endif
	//mosquitto_threaded_set(mqttCtx->mosq, true);

	mqttCtx->mqttTimeout = g_timeout_add_seconds(MQTT_DEAD_TIMEOUT, &privMqttTimeout, mqttCtx);

	if((error = mosquitto_loop_start(mqttCtx->mosq))) {
#ifdef DEBUG
		switch(error) {
			case MOSQ_ERR_SUCCESS:
				printf("Mosquitto loop start: success\n");
			break;
			case MOSQ_ERR_INVAL:
				printf("Mosquitto loop start: invalid parameters\n");
			break;
			case MOSQ_ERR_NOT_SUPPORTED:
				printf("Mosquitto loop start: threads not supported\n");
			break;
			default:
				printf("Mosquitto loop start: unknown error[%d]\n", error);
			break;
		}
#endif
		mqttFree(mqttCtx);
		return NULL;
	}

	return mqttCtx;
}

bool mqttSubscribe(MqttCtx *mqttCtx, char *topic) {
	int error;
	if((error = mosquitto_subscribe(mqttCtx->mosq, NULL, topic, 2))) {
#ifdef DEBUG
		switch(error) {
			case MOSQ_ERR_SUCCESS:
				printf("Mosquitto subscribe: Success\n");
			break;
			case MOSQ_ERR_INVAL:
				printf("Mosquitto subscribe: Invalid parameters\n");
			break;
			case MOSQ_ERR_NOMEM:
				printf("Mosquitto subscribe: Out of memory\n");
			break;
			case MOSQ_ERR_NO_CONN:
				printf("Mosquitto subscribe: No connection\n");
			break;
		}
#endif
		return false;
	}
	return true;
}

bool mqttPublish(MqttCtx *mqttCtx, char *topic, char *msg) {
	int error;
	if((error = mosquitto_publish(mqttCtx->mosq, NULL, topic, strlen(msg), (void *)msg, 0, false))) {
#ifdef DEBUG
		switch(error) {
			case MOSQ_ERR_SUCCESS:
				printf("Mosquitto publish: Success\n");
			break;
			case MOSQ_ERR_INVAL:
				printf("Mosquitto publish: Invalid parameters\n");
			break;
			case MOSQ_ERR_NOMEM:
				printf("Mosquitto publish: Out of memory\n");
			break;
			case MOSQ_ERR_NO_CONN:
				printf("Mosquitto publish: No connection\n");
			break;
			case MOSQ_ERR_PROTOCOL:
				printf("Mosquitto publish: Protocol error\n");
			break;
			case MOSQ_ERR_PAYLOAD_SIZE:
				printf("Mosquitto publish: Payload too large\n");
			break;
		}
#endif
		return false;
	}
	return true;
}

bool mqttFree(MqttCtx *mqttCtx) {
	int error;
	if(mqttCtx == NULL)
		return true;
	if(mqttCtx->mqttTimeout != 0) {
		g_source_remove(mqttCtx->mqttTimeout);
		mqttCtx->mqttTimeout = 0;
	}
	if(mqttCtx->mosq != NULL) {
		if((error = mosquitto_disconnect(mqttCtx->mosq))) {
#ifdef DEBUG
			switch(error) {
				case MOSQ_ERR_SUCCESS:
					printf("Mosquitto disconnect: Success\n");
				break;
				case MOSQ_ERR_INVAL:
					printf("Mosquitto disconnect: Invalid parameters\n");
				break;
				case MOSQ_ERR_NO_CONN:
					printf("Mosquitto disconnect: Not connected\n");
				break;
				default:
					printf("Mosquitto disconnect: Other error\n");
				break;
			}
#endif
		}
		if((error = mosquitto_loop_stop(mqttCtx->mosq, true))) {
#ifdef DEBUG
			switch(error) {
				case MOSQ_ERR_SUCCESS:
					printf("Mosquitto stop loop: Success\n");
				break;
				case MOSQ_ERR_INVAL:
					printf("Mosquitto stop loop: Invalid parameters\n");
				break;
				case MOSQ_ERR_NOT_SUPPORTED:
					printf("Mosquitto stop loop: Threads not supported\n");
				break;
				default:
					printf("Mosquitto stop loop: Other error\n");
				break;
			}
#endif
		}
		mosquitto_destroy(mqttCtx->mosq);
		mqttCtx->mosq = NULL;
	}
	mosquitto_lib_cleanup();

	free(mqttCtx);
	return true;
}

//#endif // IOTC_CLIENT
