/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 * 
 * [any text provided by the client]
 *
 * library.c
 *	  Urmet IoT library implementation
 *
 * Authors:
 *	  Matteo Di Leo <matteo.dileo@csp.it>
 */

#include "library.h"
#include "ice.h"
#include "sssdp.h"
#include "web.h"
#include "secure.h"

struct iotcCtx {
	GMainLoop *gloop;
	bool removable;
//#ifndef IOTC_CLIENT
	char *srvIp;
	char *turnUsername;
	char *turnPassword;
	// this fields are used by device only
	MqttCtx *mqttCtx;
	struct iotcServerList *serversList;
	char *uid;
	char *CAFile;
	char *CAPath;
	char *crtFile;
	char *keyFile;
	char *pKey;
//#endif // IOTC_CLIENT
};

struct iotcAgent {
	IceAgent *iceAgent;
	struct connectUserData *connectUserData;
	bool removable;
};

struct connectUserData {
	char *uid;
	IotcAgent *iotcAgent;
	const char *(*getRemoteSdp)(char *uid, char *localSdp, void *userData);
	void (*connectionStatusCb)(IotcAgent *iotcAgent, const char *status,
			ConnectionType connType, char *remoteIp, void *userData);
	void *userData;
};

extern int lPort;

//#ifndef IOTC_CLIENT
IOTC_PRIVATE void connectToServers(IotcCtx *ctx);
IOTC_PRIVATE void manageSSDPServer(IotcCtx *ctx);

IOTC_PRIVATE void deviceStatusChangedCb(IotcCtx *ctx, IceAgent *iceAgent, const char *status, void *userData,
		ConnectionType connType, char *remoteIp) {
#ifdef DEBUG
	printf("STATUS CB: %s\n", status);
#endif
	if(status != NULL &&
			(strstr(status, "timeout") == status || strstr(status, "failed") == status)) {
		iceFree(iceAgent);
		free((int *)userData);
	}
#ifdef DEBUG
	switch(connType) {
		case CONNECTION_NONE:
			printf("Connection NONE!\n");
		break;
		case CONNECTION_LAN:
			printf("Connection LAN!\n");
		break;
		case CONNECTION_P2P:
			printf("Connection P2P!\n");
		break;
		case CONNECTION_RELAY:
			printf("Connection RELAY!\n");
		break;
	}
#endif
}

IOTC_PRIVATE void deviceReadyCb(IotcCtx *ctx, IceAgent *iceAgent, char *localSdp, void *userData) {
	// TODO (malloc strlen(uid) + strlen("/server/") + MAX_INT_STRLEN + strlen('\0'))
	int len = strlen(ctx->uid) + 8 + 10 + 1;
	char *topic = (char *)malloc(len);
	snprintf(topic, len, "%s/server/%d", ctx->uid, *((int *)userData));
#ifdef DEBUG
	printf("%s : %s\n", topic, localSdp);
#endif
	if(ctx != NULL && ctx->mqttCtx != NULL)
		mqttPublish(ctx->mqttCtx, topic, localSdp);
	free(topic);
}

IOTC_PRIVATE gboolean reconnectMqttTimeoutCb(gpointer userData) {
	connectToServers((IotcCtx *)userData);
	return G_SOURCE_REMOVE;
}

IOTC_PRIVATE gboolean freeAndReconnectMqttTimeoutCb(gpointer userData) {
	IotcCtx *ctx = (IotcCtx *)userData;
	mqttFree(ctx->mqttCtx);
	ctx->mqttCtx = NULL;
	if(ctx->srvIp) {
		free(ctx->srvIp);
		ctx->srvIp = NULL;
	}
	if(ctx->turnUsername) {
		free(ctx->turnUsername);
		ctx->turnUsername = NULL;
	}
	if(ctx->turnPassword) {
		free(ctx->turnPassword);
		ctx->turnPassword = NULL;
	}
	if(ctx->serversList != NULL)
		iotcServerListDeleteFirst(&(ctx->serversList));
	g_timeout_add_seconds(1, &reconnectMqttTimeoutCb, ctx);
	return G_SOURCE_REMOVE;
}

IOTC_PRIVATE void deviceMqttDisconnectCb(MqttCtx *mqttCtx, void *userData, int rc) {
	IotcCtx *ctx = (IotcCtx *)userData;
	// I'm on mqtt loop thread, so can not stop it from here:
	// attach a callback on gmain loop and stop it there
	g_timeout_add_seconds(1, &freeAndReconnectMqttTimeoutCb, ctx);
}

IOTC_PRIVATE void deviceMqttMessageCb(MqttCtx *mqttCtx, void *userData, const struct mosquitto_message *message) {
#ifdef DEBUG
	printf("MQTT Received: ");
	fwrite(message->payload, sizeof(char), message->payloadlen, stdout);
	printf("\n");
#endif

	IotcCtx *ctx = (IotcCtx *)userData;

	// payload is ConnectionId RemoteSdp get ConnectionId to be used for publish
	char *sdpStart = strstr((char *)(message->payload), " ");
	if(sdpStart == NULL || sdpStart >= ((char*)message->payload + message->payloadlen)) {
#ifdef DEBUG
		printf("Cannot parse mqtt message...\n");
#endif
		return;
	}
	sdpStart++;
	int *mqttConnectionId = (int *)malloc(sizeof(int));
	*mqttConnectionId = atoi(message->payload);
	int sdpLength = message->payloadlen - (sdpStart-(char *)message->payload);

	// initalize device agent
	IceAgent *iceAgent = iceNew(ctx, ctx->gloop, ctx->srvIp, 3478, ctx->turnUsername, ctx->turnPassword,
			deviceReadyCb, deviceStatusChangedCb, mqttConnectionId);
	if(iceAgent == NULL) {
#ifdef DEBUG
		printf("Agent fail...\n");
#endif
		return;
	}

	// set remote sdp
	char *remoteSdp = (char *)malloc(sdpLength + 3);
	snprintf(remoteSdp, sdpLength + 1, "%s", sdpStart);
	remoteSdp[sdpLength + 2] = '\0';
#ifdef DEBUG
	printf("Setting remote sdp to: [%s]\n", remoteSdp);
#endif

	if(!iceSetRemoteSdp(iceAgent, remoteSdp)) {
#ifdef DEBUG
		printf("Error in set remote sdp\n");
#endif
	} else {
#ifdef DEBUG
		printf("Remote SDP set!\n");
#endif
	}
	free(remoteSdp);
}

IOTC_PRIVATE void deviceMqttConnectCb(MqttCtx *mqttCtx, void *userData, int result) {
	IotcCtx *ctx = (IotcCtx *)userData;
	if(!result && ctx != NULL && ctx->mqttCtx != NULL) {
		// connection OK
		if(!mqttSubscribe(ctx->mqttCtx, ctx->uid)) {
#ifdef DEBUG
			printf("MQTT fail to subscribe\n");
#endif
			return;
		}
		// save parameters of connected server
		if(ctx->serversList != NULL) {
			ctx->srvIp = strdup(ctx->serversList->ip);
			ctx->turnUsername = strdup(ctx->serversList->username);
			ctx->turnPassword = strdup(ctx->serversList->password);
			// free the server list
			iotcServerListFree(ctx->serversList);
			ctx->serversList = NULL;
		}
#ifdef DEBUG
		printf("[DEBUG] Registered to server %s\n", ctx->srvIp);
#endif
		webDeviceRegister(ctx->srvIp, ctx->CAFile, ctx->CAPath, ctx->crtFile, ctx->keyFile);
	} else {
		// connection failed
		iotcServerListDeleteFirst(&(ctx->serversList));
		g_timeout_add_seconds(1, &reconnectMqttTimeoutCb, ctx);
	}
}

IOTC_PRIVATE void checkCertsCb(int code, char *response, void *userData) {
	IotcCtx *ctx = (IotcCtx *)userData;
	if(response != NULL) {
#ifdef DEBUG
		if(code >= 0) {
			printf("%d %s", code, response);
		}
#endif
		if(code == 200 && ctx->pKey != NULL &&
				strlen(ctx->pKey) <= 4096) {
			// parse cacert and cert
			char *beginCrt = strstr(response, "-----BEGIN CERTIFICATE-----");
			char *endCrt = strstr(response, "-----END CERTIFICATE-----");
			if(beginCrt != NULL && endCrt != NULL && endCrt > beginCrt) {
				char *beginCa = strstr(endCrt+25, "-----BEGIN CERTIFICATE-----");
				char *endCa = strstr(endCrt+25, "-----END CERTIFICATE-----");
				if(beginCa != NULL && endCa != NULL && endCa > beginCa) {
					int lengthCrt = endCrt - beginCrt + 25;
					int lengthCa = endCa - beginCa + 25;

					if(lengthCrt <= 4096 && lengthCa <= 4096) {
						// store certs on device!
						FILE *fp = fopen(ctx->keyFile, "w+");
						if(fp != NULL) {
							fprintf(fp, "%s", ctx->pKey);
							fclose(fp);
						} else {
#ifdef DEBUG
							printf("[DEBUG] Cannot create key file\n");
#endif
						}
						fp = fopen(ctx->crtFile, "w+");
						if(fp != NULL) {
							fprintf(fp, "%.*s", lengthCrt, beginCrt);
							fclose(fp);
						} else {
#ifdef DEBUG
							printf("[DEBUG] Cannot create crt file\n");
#endif
						}
						fp = fopen(ctx->CAFile, "w+");
						if(fp != NULL) {
							fprintf(fp, "%.*s", lengthCa, beginCa);
							fclose(fp);
						} else {
#ifdef DEBUG
							printf("[DEBUG] Cannot create cacert file\n");
#endif
						}
					}
				}
			}
		}
	}
	if(ctx->pKey != NULL) {
		free(ctx->pKey);
		ctx->pKey = NULL;
	}
	g_timeout_add_seconds(1, &reconnectMqttTimeoutCb, ctx);
}

IOTC_PRIVATE void checkCerts(IotcCtx *ctx) {
	char *csr;
	struct certBundle *bundle = generateCSR(ctx->uid);
	if(bundle != NULL) {
		char *csrString = csrToString(bundle);
		if(csrString == NULL) {
			certBundleFree(bundle);
			g_timeout_add_seconds(1, &reconnectMqttTimeoutCb, ctx);
#ifdef DEBUG
			printf("[DEBUG] Cannot parse csr\n");
#endif
			return;
		}
		csr = pemToUrl(csrString);
		free(csrString);

		char post[4096];
		snprintf(post, 4096, "csr=%s", csr);
		ctx->pKey = pKeyToString(bundle);
		//httpPostAsync(SERVER_NAME, 80, "/tool/devapi/public/index.php/x509_device_register",
		//		post, checkCertsCb, ctx);
		httpsPostAsync(SERVER_NAME, 443, "/tool/devapi/public/index.php/x509_device_register", 
				NULL, 
				NULL, 
				NULL, 
				NULL,
				post, checkCertsCb, ctx);
	
		if(csr != NULL)
			free(csr);
		certBundleFree(bundle);
	} else {
		g_timeout_add_seconds(1, &reconnectMqttTimeoutCb, ctx);
#ifdef DEBUG
		printf("[ERROR] Certificate generation error");
#endif
		return;
	}
}

IOTC_PRIVATE gboolean checkCertsTimeoutCb(gpointer userData) {
	checkCerts((IotcCtx *)userData);
	return G_SOURCE_REMOVE;
}

IOTC_PRIVATE void webGetServersCb(struct iotcServerList *list, void *userData) {
	IotcCtx *ctx = (IotcCtx *)userData;
	ctx->serversList = list;
	if(ctx->serversList == NULL) {
		g_timeout_add_seconds(1, &checkCertsTimeoutCb, ctx);
	} else {
		g_timeout_add_seconds(1, &reconnectMqttTimeoutCb, ctx);
	}
}

IOTC_PRIVATE void connectToServers(IotcCtx *ctx) {
	if(ctx->serversList == NULL) {
#ifdef DEBUG
		printf("[DEBUG] Retrieving servers list...\n");
#endif
		webDeviceGetServersAsync(ctx->CAFile, ctx->CAPath,
				ctx->crtFile, ctx->keyFile, webGetServersCb, ctx);
	} else {
#ifdef DEBUG
		printf("[DEBUG] Try to connect to server: %s %s:%s\n", ctx->serversList->ip,
				ctx->serversList->username, ctx->serversList->password);
#endif
		ctx->mqttCtx = mqttNew(ctx->uid, ctx->serversList->ip, 1883,
				ctx->CAFile, ctx->CAPath, ctx->crtFile, ctx->keyFile,
				deviceMqttConnectCb, NULL,
				deviceMqttMessageCb, deviceMqttDisconnectCb, ctx);
		if(ctx->mqttCtx == NULL) {
			// connection failed
			iotcServerListDeleteFirst(&(ctx->serversList));
			g_timeout_add_seconds(1, &reconnectMqttTimeoutCb, ctx);
		}
	}
}

IOTC_PRIVATE gboolean restartSSDPServerCb(gpointer userData) {
	IotcCtx *ctx = (IotcCtx *)userData;
	manageSSDPServer(ctx);
	return G_SOURCE_REMOVE;
}

IOTC_PRIVATE void manageSSDPServer(IotcCtx *ctx) {
	// Roby: originale
	if(!startSSDPServer(ctx->gloop, "DigitalSecurityCamera", "schemas-urmet-com", "Camera", "URMET",
			WEBSERVICE_ENDPOINT, "Model 0", "0.0.1", "", "00000000000000000001", ctx->uid))
		g_timeout_add_seconds(5, &restartSSDPServerCb, ctx);

	//Roby new
	// if(!startSSDPServer(ctx->gloop, NULL, NULL, NULL, NULL,
	// 		NULL, NULL, NULL, "", "00000000000000000001", ctx->uid))
	// 	g_timeout_add_seconds(5, &restartSSDPServerCb, ctx);
}

/*
gboolean deviceKeyboardCb(GIOChannel *src, GIOCondition cond, gpointer userData) {
	char str[1024];
	fgets(str, 1024, stdin);
	printf("PRESSED: %s", str);
	// do something...
	return TRUE;
}
*/
/*
IotcCtx *ctxG;
void ctrlCHandler(int sig) {
	g_main_loop_quit(ctxG->gloop);
}
*/

int iotcInitDevice(char *uid, char *basePath) {
	printf("IOT v.%s\n", VERSION);
	g_type_init();

	IotcCtx *ctx = (IotcCtx *)malloc(sizeof(IotcCtx));
	ctx->gloop = g_main_loop_new(NULL, FALSE);
	ctx->srvIp = NULL;
	ctx->turnUsername = NULL;
	ctx->turnPassword = NULL;
	ctx->mqttCtx = NULL;
	ctx->serversList = NULL;
	ctx->uid = uid != NULL ? strdup(uid) : "DUMMY";
	ctx->pKey = NULL;

	// 11 because strlen("cacert.pem") = strlen("device.crt") = strlen("device.key") = 10 + 1 for \0
	int length = strlen(basePath) + 11;
	ctx->CAFile = (char *)malloc(length);
	snprintf(ctx->CAFile, length, "%scacert.pem", basePath);
	ctx->CAPath = NULL;
	ctx->crtFile = (char *)malloc(length+11);
	snprintf(ctx->crtFile, length, "%sdevice.crt", basePath);
	ctx->keyFile = (char *)malloc(length+11);
	snprintf(ctx->keyFile, length, "%sdevice.key", basePath);

//	startSSDPServer(ctx->gloop, "DigitalSecurityCamera", "schemas-urmet-com", "Camera", "URMET",
//			"http://www.cloud.urmet.com", "Model 0", "0.0.1", "", "00000000000000000001", ctx->uid);
	manageSSDPServer(ctx);
/*
	GIOChannel *io_stdin;
	io_stdin = g_io_channel_unix_new(fileno(stdin));
	g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc)deviceKeyboardCb, ctx);
*/

	connectToServers(ctx);
/*
	ctxG = ctx;
	signal(SIGINT, ctrlCHandler);
*/
#ifdef DEBUG
	printf("Entering main loop...\n");
#endif
	g_main_loop_run(ctx->gloop);
	printf("Exit main loop...\n");
	g_main_loop_unref(ctx->gloop);
	return 0;
}
//#endif // IOTC_CLIENT

IOTC_PRIVATE void *clientThreadInit(void *arg) {
	IotcCtx *ctx = (IotcCtx *)arg;
	g_main_loop_run(ctx->gloop);
	printf("Exit main loop...\n");
	g_main_loop_unref(ctx->gloop);
	ctx->removable = true;
	printf("Freed main loop...\n");
	return NULL;
}

IOTC_PRIVATE void clientStatusChangedCb(IotcCtx *ctx, IceAgent *iceAgent, const char *status, void *userData,
		ConnectionType connType, char *remoteIp) {	
	
	struct connectUserData *data = (struct connectUserData *)userData;
	void (*connectionStatusCb)(IotcAgent *iotcAgent, const char *status,
			ConnectionType connType, char *remoteIp, void *userData) = data->connectionStatusCb;
	if(connectionStatusCb != NULL)
		connectionStatusCb(data->iotcAgent, status, connType, remoteIp, data->userData);
/*	if(status != NULL &&
			(strstr(status, "timeout") == status || strstr(status, "failed") == status)) {
		iceStop(iceAgent);
	}*/
	
	char filename[256];
	memset(filename, 0x00, sizeof(filename));
	sprintf(filename, "/usr/local/urmetiotc_x86_64/bin/db_update_status.sh %s %s %d", data->uid, status, lPort);
	printf("exec > %s\n", filename);
	int r = system(filename);
	printf("exec returned %d\n", r);
	if (!strcmp(status, "timeout") || 
			!strcmp(status, "failed") || 
			!strcmp(status, "offline") ) {
#ifdef DEBUG
		printf("CLIENT CONNECTION FAILURE !!! EXIT !!!\n");
#endif
		exit(1);	
	}
	else {
#ifdef DEBUG		
		printf("STATUS %s,PROCESS NEXT STATUS ...\n", status);
#endif
	}
}

IOTC_PRIVATE void clientReadyCb(IotcCtx *ctx, IceAgent *iceAgent, char *localSdp, void *userData) {
	struct connectUserData *data = (struct connectUserData *)userData;
	const char *remoteSdp = NULL;
	const char *(*getRemoteSdp)(char *uid, char *localSdp, void *userData) = data->getRemoteSdp;	
	if(getRemoteSdp != NULL)
		remoteSdp = getRemoteSdp(data->uid, localSdp, data->userData);
	if(remoteSdp != NULL) {
		iceSetRemoteSdp(iceAgent, remoteSdp);
		free((void *)remoteSdp);
	} else {
		void (*connectionStatusCb)(IotcAgent *iotcAgent, const char *status,
				ConnectionType connType, char *remoteIp, void *userData) = data->connectionStatusCb;
		if(connectionStatusCb != NULL)
			connectionStatusCb(data->iotcAgent, "failed", CONNECTION_NONE, "", data->userData);
//		iceStop(iceAgent);
	}
	data->iotcAgent->removable = true;
}

IotcCtx *iotcInitClient() {
	printf("IOT v.%s\n", VERSION);
#if !GLIB_CHECK_VERSION(2, 36, 0)
#endif
	g_type_init();
	GMainLoop *gloop = g_main_loop_new(NULL, FALSE);
	IotcCtx *ctx = (IotcCtx *)malloc(sizeof(IotcCtx));
	ctx->gloop = gloop;
	ctx->removable = false;
	pthread_t threadId;
	pthread_create(&threadId, NULL, &clientThreadInit, ctx);
	pthread_detach(threadId);
	return ctx;
}

void iotcDeinit(IotcCtx *iotcCtx) {
	g_main_loop_quit(iotcCtx->gloop);
	while(!iotcCtx->removable)
		sleep(1);
	free(iotcCtx);
}

IotcAgent *iotcConnect(IotcCtx *ctx, const char *uid,
		const char *serverIp, const char *serverUsername, const char *serverPassword,
		const char *(*getRemoteSdp)(char *uid, char *localSdp, void *userData),
		void (*connectionStatusCb)(IotcAgent *iotcAgent, const char *status,
				ConnectionType connType, char *remoteIp, void *userData),
		void *userData) {
	struct connectUserData *connectUserData = (struct connectUserData *)malloc(sizeof(struct connectUserData));
	connectUserData->uid = uid != NULL ? strdup(uid) : NULL;
	connectUserData->getRemoteSdp = getRemoteSdp;
	connectUserData->connectionStatusCb = connectionStatusCb;
	connectUserData->userData = userData;
	IotcAgent *iotcAgent = (IotcAgent *)malloc(sizeof(IotcAgent));
	connectUserData->iotcAgent = iotcAgent;
	iotcAgent->connectUserData = connectUserData;
	iotcAgent->iceAgent = iceNew(ctx, ctx->gloop, serverIp, 3478, serverUsername, serverPassword,
			clientReadyCb, clientStatusChangedCb, (void *)connectUserData);
	iotcAgent->removable = false;
	if(iotcAgent->iceAgent == NULL) {
#ifdef DEBUG
		printf("Agent fail...\n");
#endif
		if(connectUserData->uid != NULL)
			free(connectUserData->uid);
		free(connectUserData);
		free(iotcAgent);
		return NULL;
	}
	return iotcAgent;
}

void iotcDisconnect(IotcAgent *iotcAgent) {
	iotcAgent->connectUserData->getRemoteSdp = NULL;
	iotcAgent->connectUserData->connectionStatusCb = NULL;
	while(!iotcAgent->removable)
		sleep(1);
	iceFree(iotcAgent->iceAgent);
	if(iotcAgent->connectUserData->uid != NULL)
		free(iotcAgent->connectUserData->uid);
	free(iotcAgent->connectUserData);
	free(iotcAgent);
}

bool portMap(IotcAgent *iotcAgent, unsigned short localPort, unsigned short remotePort,
		TunnelProtocols proto) {
	return icePortMap(iotcAgent->iceAgent, localPort, remotePort, proto);
}

IOTC_PRIVATE void discoveryEndCb(struct deviceDiscoveredList *list, void *userData) {
	((void (*)(struct deviceDiscoveredList *))userData)(list);
}

bool lanDiscovery(IotcCtx *ctx, void (*onDiscoveryResults)(struct deviceDiscoveredList *)) {
//	return startSSDPDiscovery(ctx->gloop, "urn:schemas-urmet-com:device:DigitalSecurityCamera:1", discoveryEndCb, (void *)onDiscoveryResults); 	//Roby: originale

	//Roby: aggiunta ricerca di device 1061
//    return startSSDPDiscovery(ctx->gloop, "device:U1061-4e64bd", discoveryEndCb, (void *)onDiscoveryResults);	// Roby: ONLY 1061 intrusion system (??  USN: uuid:U1061-4e64bd::upnp:rootdevice ?? basta la parte dopo "uuid:". Quella viene aggiunta in sssdp.c@263 )
//	return startSSDPDiscovery(ctx->gloop, "urn:schemas-upnp-org:device:DigitalSecurityCamera:1", discoveryEndCb, (void *)onDiscoveryResults); 	//Roby: originale	
//	return startSSDPDiscovery(ctx->gloop, "upnp:rootdevice", discoveryEndCb, (void *)onDiscoveryResults); 	
//    return startSSDPDiscovery(ctx->gloop, "ssdp:all", discoveryEndCb, (void *)onDiscoveryResults);	// Roby: removed filter to include ALL devices


    //bool ssdpDiscovery1 = startSSDPDiscovery(ctx->gloop, "upnp:rootdevice", discoveryEndCb, (void *)onDiscoveryResults); 		//Roby: start search for rootdevices (to include results from Intrusion Systems as 1061, 1067 etc..)
//	return startSSDPDiscovery(ctx->gloop, "upnp:rootdevice", discoveryEndCb, (void *)onDiscoveryResults); //ok
	return startSSDPDiscovery(ctx->gloop, "ssdp:all", discoveryEndCb, (void *)onDiscoveryResults); 
}


char *getMyLocalIP() {
//#ifndef IOTC_CLIENT
#ifndef IFADDRS_NOT_SUPPORTED
	struct ifaddrs *ifAddrStruct = NULL;
	struct ifaddrs *ifa = NULL;
	void *tmpAddrPtr = NULL;
	char *addressBuffer = (char *)malloc(INET_ADDRSTRLEN);
	if(addressBuffer == NULL)
		return NULL;
	char *addressBuffer6 = (char *)malloc(INET6_ADDRSTRLEN);
	if(addressBuffer6 == NULL)
		return NULL;

	getifaddrs(&ifAddrStruct);

	for(ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
		if(!ifa->ifa_addr)
			continue;
		if(ifa->ifa_addr->sa_family == AF_INET) {
			// is a valid IP4 Address
			tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
			inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
			if(strcmp(addressBuffer, "127.0.0.1") != 0) {
				if(ifAddrStruct!=NULL)
					freeifaddrs(ifAddrStruct);
				free(addressBuffer6);
				return addressBuffer;
			}
		} else if(ifa->ifa_addr->sa_family == AF_INET6) {
			// is a valid IP6 Address
			tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
			inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer6, INET6_ADDRSTRLEN);
			if(strcmp(addressBuffer6, "::1") != 0) {
				if(ifAddrStruct!=NULL)
					freeifaddrs(ifAddrStruct);
				free(addressBuffer);
				return addressBuffer6;
			}
		}
	}
	if(ifAddrStruct!=NULL)
		freeifaddrs(ifAddrStruct);
	free(addressBuffer);
	free(addressBuffer6);
#endif // IFADDRS_NOT_SUPPORTED
//#endif // IOTC_CLIENT
	return NULL;
}

/*
char *getMyLocalIP() {
	//return "10.2.1.9";
	return "10.2.1.3";
}
*/