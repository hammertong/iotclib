#include <stdio.h>
#include "library.h"
#include "ice.h"
#include "sssdp.h"
#include "secure.h"

int agentSend = 2;
int lPort, rPort;
TunnelProtocols gProto;

void discoveryCb(struct deviceDiscoveredList *list) {
	struct deviceDiscoveredList *l;
	for(l=list; l; l=l->next) {
		printf("R: %s %s\n", l->uuid, l->ip);
	}
	l = list;
	while(l!=NULL) {
		struct deviceDiscoveredList *tmp = l;
		l = l->next;
		free(tmp->uuid);
		free(tmp->urnDeviceType);
		free(tmp->ip);
		free(tmp);
	}
}

const char *getRemoteSdp(char *uid, char *localSdp, void *userData) {
	printf("getRemoteSdp: %s, %s\n", uid, localSdp);
	char *response;
	char *sdpEncoded = pemToUrl(localSdp);
	char *pathAndGet = (char *)malloc(strlen(uid) + strlen(sdpEncoded) + 70);
	sprintf(pathAndGet, "/tool/webapi/private/index.php/iotc_connect/?uid=%s&ice_sdp_cli=%s", uid, sdpEncoded);
	free(sdpEncoded);

	httpsPost("www.cloud.urmet.com", 443, pathAndGet, &response,
			NULL, NULL, NULL, NULL,
			"httpd_username=dileo&httpd_password=dileo");
	free(pathAndGet);
	if(response == NULL) {
		printf("[ERROR] response is null...\n");
		return NULL;
	}

	char *startDevSdp = strstr(response, "ice_sdp_dev");
	if(startDevSdp != NULL) {
		startDevSdp += 14;
		startDevSdp[strlen(startDevSdp)-2] = '\0';
		char *ret = strdup(startDevSdp);
		free(response);
		return ret;
	}
	free(response);
	return NULL;
}

void statusCb(IotcAgent *iotcAgent, const char *status, ConnectionType connType, char *remoteIp, void *userData) {
	printf("STATUS: %s\n", status);
	if(strstr(status, "ready") != NULL) {
		portMap(iotcAgent, lPort, rPort, gProto);
	}
	if(remoteIp != NULL)
		printf("REMOTE IP: %s\n", remoteIp);
}

IotcAgent *iotcAgent = NULL;


void ctrlCHandler(int sig) {
	printf("Ctrl-C pressed! Stop agent...");
	if(iotcAgent != NULL) {
		iotcDisconnect(iotcAgent);
		iotcAgent = NULL;
	}
	printf("stopped\n");
}


int testClient(char *uid, int localPort, int remotePort, TunnelProtocols proto) {

	lPort = localPort;
	rPort = remotePort;
	gProto = proto;

    signal(SIGINT, ctrlCHandler);

	// listen keyboard input
	while(true) {

		IotcCtx *ctx = iotcInitClient();
		
		if (ctx == NULL) {
			printf("Cannot initialize client");
			return -1;
		}

		printf("LAN discovery started...\n");
		lanDiscovery(ctx, discoveryCb);

		printf("Connecting to %s...\n", uid);
		iotcAgent = iotcConnect(
				ctx, 
				uid, 
				"35.195.29.62", 
				"admin", 
				"iotc$urm_2016",
				getRemoteSdp, 
				statusCb, 
				NULL);

		while(iotcAgent != NULL) sleep(1);

		iotcDeinit(ctx);

	}

	return 0;

}

