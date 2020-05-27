/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 * 
 * [any text provided by the client]
 * 
 * <filename>
 * 	<description>
 * 
 * Authors:
 * 	Matteo Di Leo <matteo.dileo@csp.it>
 * 
 */

/**
 * @file main.c
 * @author Matteo Di Leo
 * @date 11/02/2015
 * @brief A simple main to test functionalities
 *
 * This simple main will be probably removed from production version.
 * It is used to test functionalities.
 */

//#ifndef IOTC_CLIENT

#include <stdio.h>
#include "library.h"
#include "secure.h"
#include "web.h"

#include <sys/time.h>


#if 0
int portToBeMapped;

static GMainLoop *gloop;

void mqttMessageCb(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
	printf("Received: ");
	fwrite(message->payload, sizeof(char), message->payloadlen, stdout);
	printf("\n");
}

void testMqtt() {
	int i, j;
	for(j=0; j<5; j++) {
		struct mosquitto *mosq = mqttNew("DEVICE1", "cn1-urmetiotc.csp.it", 1883, "/usr/local/apache2/ssl/crt/ca.crt", NULL, "/usr/local/apache2/ssl/client/client.crt", "/usr/local/apache2/ssl/client/client.key", mqttMessageCb);
		if(!mqttSubscribe(mosq, "DEVICE1"))
			return;

		for(i = 0; i < 3; i++) {
			if(!mqttPublish(mosq, "DEVICE1/server", "Ciao Server!"))
				return;
			sleep(1); printf("."); fflush(stdout);
		}
		printf("\n");
		if(!mqttFree(mosq))
			return;
	}
	printf("MQTT test done\n");
}

void testSecure() {
	char *csr = "";
	struct certBundle *bundle = generateCSR("FVP9B9YE53A1VMPKPVPJ");

	if(bundle != NULL) {
		printPkey(stdout, bundle);
		printCsr(stdout, bundle);
		printCert(stdout, bundle);

//		printf("%s\n", pKeyToString(bundle));
//		printf("%s\n", pemToUrl(csrToString(bundle)));
		csr = pemToUrl(csrToString(bundle));
//		printf("%s\n", certToString(bundle));

		certBundleFree(bundle);
	} else {
		printf("bundle error\n");
	}

	char post[2048];
	snprintf(post, 2048, "csr=%s", csr);

	char *buffer = NULL;

	// Require to sign my csr
	httpPost("urmetiotc.csp.it", "/tool/devapi/public/index.php/x509_device_register", &buffer, post);

/*	httpsGet("urmetcloud.csp.it", "/secure/", &buffer, "/usr/local/apache2/ssl/crt/ca.crt", "../certs/",
		"/usr/local/apache2/ssl/client/client.crt",
		"/usr/local/apache2/ssl/client/client.key");

	httpsGet("urmetiotc.csp.it", 4343, "/", &buffer, "/usr/local/src/iotl/certs/ca.pem", NULL,
		"/usr/local/src/iotl/certs/cert.pem",
		"/usr/local/src/iotl/certs/key.pem");
*/
	if(csr != NULL)
		free(csr);
	if(buffer != NULL)
		free(buffer);
	return;
}

void sendSdp(IceAgent *iceAgent, char *localSdp) {
	printf("%s\n", localSdp);
	if(!iceSetRemoteSdp(iceAgent, "zVHM U886lpiuytOlsDwTzmmHCS 1,2013266431,194.116.4.80,45850,host"))
		printf("Error in set remote sdp\n");
	else
		printf("Remote SDP set!\n");
}

void testIce() {
	GMainLoop *gloop = gloopNew();
	IceAgent *iceAgent = iceNew(gloop, "194.116.4.80", 3478, "topolino", "pluto", sendSdp);
	if(iceAgent == NULL)
		return;
	g_main_loop_run(gloop);

	g_main_loop_unref(gloop);
	iceFree(iceAgent);
	printf("ICE test done\n");
}

void readyCb(IceAgent *iceAgent, char *localSdp) {
	printf("%s\n", localSdp);
}

IceAgent *iceAgent;
int agentSend = 2;
void mqttMessageCbDevice(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
	printf("MQTT Received: ");
	fwrite(message->payload, sizeof(char), message->payloadlen, stdout);
	printf("\n");

//	if(agentSend != 0) {
		IceAgent *mIceAgent = iceNew(gloop, "194.116.4.80", 3478, "topolino", "pluto", readyCb);
		if(mIceAgent == NULL) {
			printf("Agent fail...\n");
			return;
		}
		agentSend = 1;
		char *remoteSdp = (char *)malloc(message->payloadlen + 2);
		snprintf(remoteSdp, message->payloadlen + 1, "%s", (char *)(message->payload));
		remoteSdp[message->payloadlen + 2] = '\0';

		printf("Setting remote sdp to: [%s]\n", remoteSdp);

		if(!iceSetRemoteSdp(mIceAgent, remoteSdp))
			printf("Error in set remote sdp\n");
		else
			printf("Remote SDP set!\n");
		free(remoteSdp);
		agentSend = 0;
//	} else {
//		iceSend(iceAgent, 0, message->payloadlen, message->payload);
//		nice_agent_send(iceAgent->agent, 1, 1, message->payloadlen, message->payload);
//		iceFree(iceAgent);
//	}
}

void testDevice() {
	char *uid = "device1";
	struct mosquitto *mosq = mqttNew(uid, "cn2-urmetiotc.csp.it", 1883, "/usr/local/iotc-dependencies/certs/cacert.pem", NULL, "/usr/local/iotc-dependencies/certs/device1.crt", "/usr/local/iotc-dependencies/certs/device1.key", mqttMessageCbDevice);
//	struct mosquitto *mosq = mqttNew(uid, "cn2-urmetiotc.csp.it", 1883, "./cacert.pem", NULL, "./device1.crt", "./device1.key", mqttMessageCbDevice);
	if(mosq == NULL || !mqttSubscribe(mosq, uid)) {
		printf("Mosquitto fail\n");
		return;
	}
	gloop = gloopNew();
	if(gloop == NULL) {
		printf("Main loop fail\n");
		return;
	}

//	startSSDPServer(gloop, "DigitalSecurityCamera", "schemas-urmet-com", "Telecamera", "CSP",	//Roby: originale
//			"http://www.csp.it", "Model 0", "0.0.1", "", "00000000000000000001", uid);			//Roby: originale
	startSSDPServer(gloop, "", "", "", "", 		// Roby: new
			"", "", "", "", "", uid);			// Roby: new

	printf("Starting main loop...\n");
	g_main_loop_run(gloop);
	printf("Exit main loop...\n");
	g_main_loop_unref(gloop);
}

void mqttMessageCbClient(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
	printf("MQTT Received: ");
	fwrite(message->payload, sizeof(char), message->payloadlen, stdout);
	printf("\n");

	if(agentSend == 2) {
		char *remoteSdp = (char *)malloc(message->payloadlen + 2);
		snprintf(remoteSdp, message->payloadlen + 1, "%s", (char *)(message->payload));
		remoteSdp[message->payloadlen + 2] = '\0';

		printf("Setting remote sdp to: [%s]\n", remoteSdp);

		if(!iceSetRemoteSdp(iceAgent, remoteSdp))
			printf("Error in set remote sdp\n");
		else
			printf("Remote SDP set!\n");
		free(remoteSdp);
		agentSend = 1;
	} else if(agentSend == 1) {
//		char *msg = malloc(1024);
//		msg[0] = 0;
//		msg[1] = 1;
//		msg[2] = 80;
//		msg[3] = 0;
//		msg[4] = 0;
//		msg[5] = 80;
//		msg[6] = 1;
////		nice_agent_send(iceAgent->agent, 1, 1, 1024, msg);
//		iceSend(iceAgent, 0, 1024, msg);
		portMap(iceAgent, 8080, portToBeMapped, P2P_TCP);
		agentSend = 0;
	} else {
		// send all messages to channel 1
//		((char *)(message->payload))[0] = 1;
//		nice_agent_send(agent, 1, 1, message->payloadlen, message->payload);
		char *msg = "GET / HTTP/1.1\nUser-Agent: IoTl/0\nAccept: */*\nHost: 127.0.0.1\nConnection: Close\nContent-Length: 0\n\n";

//		nice_agent_send(iceAgent->agent, 1, 2, strlen(msg), msg);
		int a = iceSend(iceAgent, -2/*1*/, strlen(msg), msg);
		printf("Ice Sent? %d\n", a);
	}
}

void discoverEndCb(struct deviceDiscoveredList *list) {
	struct deviceDiscoveredList *l;
	for(l=list; l; l=l->next) {
		printf("R: %s %s\n", l->uuid, l->ip);
	}
}

void testClient() {
	// mqtt is useless for client, here it is used for test purpose only
	char *uid = "device2";
	struct mosquitto *mosq = mqttNew(uid, "cn2-urmetiotc.csp.it", 1883, "/usr/local/iotc-dependencies/certs/cacert.pem", NULL, "/usr/local/iotc-dependencies/certs/device2.crt", "/usr/local/iotc-dependencies/certs/device2.key", mqttMessageCbClient);
	if(mosq == NULL || !mqttSubscribe(mosq, uid)) {
		printf("Mosquitto fail\n");
		return;
	}
	gloop = gloopNew();
	if(gloop == NULL) {
		printf("Main loop fail\n");
		return;
	}
	printf("Starting main loop...\n");

	iceAgent = iceNew(gloop, "194.116.2.83", 3478, "admin", "iotc$urm_2016", readyCb);
	if(iceAgent == NULL) {
		printf("Agent fail...\n");
		return;
	}

	printf("SSDP Discovery start: %d\n",
		startSSDPDiscovery(gloop, NULL, discoverEndCb));			//Roby: new
//		startSSDPDiscovery(gloop, "urn:schemas-urmet-com:device:DigitalSecurityCamera:1", discoverEndCb));	//Roby: originale

	g_main_loop_run(gloop);
	printf("Exit main loop...\n");
	g_main_loop_unref(gloop);
	g_object_unref(iceAgent);
}
#endif

int testDevice(int argc, char *argv[]);
int testClient(char *uid, int localPort, int remotePort, TunnelProtocols proto);

#ifdef DEBUG
#define __USE_GNU
#include <dlfcn.h>
int depth = -1;
// add logs to entry and exit point of each function
void __cyg_profile_func_enter(void *, void *) __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void *, void *) __attribute__((no_instrument_function));
void my_common_log_function(void *, void *, char *) __attribute__((no_instrument_function));
void __cyg_profile_func_enter(void *func,  void *caller) {
	depth++;
	my_common_log_function(func, caller, "Enter");
}
void __cyg_profile_func_exit(void *func, void *caller) {
	depth--;
	my_common_log_function(func, caller, "Exit ");
}

void my_common_log_function(void *func, void *caller, char *action) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	int ret;
	const char *funcName, *callerName;
	Dl_info info;

	memset(&info, 0, sizeof(info));
	ret = dladdr(func, &info);
	if(ret != 0 && info.dli_sname != NULL)
		funcName = info.dli_sname;
	else
		funcName = "unknown";

	memset(&info, 0, sizeof(info));
	ret = dladdr(caller, &info);
	if(ret != 0 && info.dli_sname != NULL)
		callerName = info.dli_sname;
	else
		callerName = "unknown";
	printf("[%ld] %s: %3d %8p %8p (%s) (%s)\n",
			(long)tv.tv_sec, action, depth, func, caller, funcName, callerName);
}
#endif

/**
 * The main program
 */
int main(int argc, char *argv[]) {
/*	char *postMsg;
	if(argc < 2) {
		printf("Missing argument\nUSAGE:\n  %s <path> <post data>\n", argv[0]);
		return -1;
	}
	if(argc > 2) {
		postMsg = argv[2];
	} else {
		postMsg = "";
	}
	char *buffer = NULL;
	httpsGet("urmetiotc.csp.it", argv[1], &buffer,
			"/usr/local/iotc-dependencies/certs/cacert.pem", NULL,
			"/usr/local/iotc-dependencies/certs/device1.crt",
			"/usr/local/iotc-dependencies/certs/device1.key"
//			, postMsg
			);
	return 0;*/
//	testSecure();
//	return 0;
//	testMqtt();
//	testIce();

	printf("Version %s\n", VERSION);
	printf("Auth web server: %s\n", SERVER_NAME);
	printf("Iotc web server: %s:%d\n", IOTC_VHOST, IOTC_VHOST_PORT);

	if(strstr(argv[0], "device") != 0) {
		return testDevice(argc, argv);
	} else if(strstr(argv[0], "client") != 0) {

		printf("Ice server: %s\n", ICE_SERVER);		
		if(argc != 5 || strlen(argv[1]) > 20 ||
				(strstr(argv[4], "TCP") == NULL && strstr(argv[4], "RTSP") == NULL)) {
			printf("Usage:\n\t%s UID L_PORT R_PORT MODE\n\n\tUID: an alphanumeric string of max 20 characters (example: EFY98NRY9VU1BMPKPVC1)\n\tL_PORT: the port on localhost\n\tR_PORT: the port of the service on remote host\n\tMODE: TCP or RTSP (upper-case)\n", argv[0]);
			return 1;
		}
/*		int remotePort, localPort;
		if(argc > 2)
			remotePort = atoi(argv[2]);
		else
			remotePort = 80;

		if(argc > 3)
			localPort = atoi(argv[3]);
		else
			localPort = 8080;
		return testClient(argv[1], localPort, remotePort);*/
		if(strstr(argv[4], "TCP") != NULL) {
			testClient(argv[1], atoi(argv[2]), atoi(argv[3]), P2P_TCP);
		} else if(strstr(argv[4], "RTSP") != NULL) {
			testClient(argv[1], atoi(argv[2]), atoi(argv[3]), P2P_RTSP);
		}
	}

	return 0;
}

//#endif // IOTC_CLIENT
