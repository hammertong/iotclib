/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 * 
 * [any text provided by the client]
 *
 * ice.c
 *      Urmet IoT ICE implementation
 *
 * Authors:
 *      Matteo Di Leo <matteo.dileo@csp.it>
 */

#include "library.h"
#include "ice.h"

#ifdef IFADDRS_NOT_SUPPORTED
#include <sys/ioctl.h>
#include <net/if.h>
#else
#include <ifaddrs.h>
#endif

#define ICE_MAX_CH 50
#define BUFFER_LEN 1550 // 1550
#define ICE_TIMEOUT 30 // timeout for custom ping used to test ice connection (should be > 2*ICE_TIMEOUT_INTERVAL)
#define ICE_TIMEOUT_INTERVAL 5 // interval for send ping used to test ice connection

#ifdef DEBUG
long sentSocket = 0, recvSocket = 0, sentIce = 0, recvIce = 0;
#endif

//PRIVATE
/*
 * All actions are activated by client communicating on ice channel 0 with device.
 * Custom protocol:
 * |--------|--------|--------|--------|--------|--------|--------|--------|
 * |   0    | action | new ch |     src port    |     dst port    | proto  |	// P2P_TUNNEL_MAP
 * |   0    | action |   ch   |							// P2P_TUNNEL_SHUT
 * |   0    | action |								// P2P_TUNNEL_PING
 * |--------|--------|--------|--------|--------|--------|--------|--------|
 */

struct connectionInfo {
	int channel;
	int sock;
	struct sockaddr_in srcAddr;
	struct sockaddr_in dstAddr;
	TunnelProtocols proto;
	NiceAgent *agent;
	GSocketConnection *connection;
	guint gsource;
	char *buffer;
	int sentBytes;
	int unsentBytes;
	bool *pendingSend;
	gulong iceCanWriteSignalHandler;
	struct connectionList *rtpChList;
};
typedef struct connectionInfo ConnectionInfo;

struct connectionList {
	ConnectionInfo *value;
	struct connectionList *next;
};

struct socketServiceList {
	GSocketService *service;
	struct iceAgentClient *iac;
	struct socketServiceList *next;
};

/**
 * To create and initialize IceAgent use the function iceNew()
 * To destroy IceAgent use iceFree()
 * Do not reuse the same IceAgent for reconnection, even to the same end point,
 * when some error occurs and connection is lost, just destroy and recreate.
 */
struct iceAgent {
	NiceAgent *agent;
	IotcCtx *ctx;
	ConnectionInfo *conns;
	bool pendingSend;
	char *recvBuffer;
	int readedBytes;
	int packetSize;
	time_t *timeout;
	guint gsourceTimeout;
	struct socketServiceList *socketServiceList;
	void (*onReady)(IotcCtx *ctx, IceAgent *, char *, void *);
	void (*onStatusChanged)(IotcCtx *ctx, IceAgent *, const char *, void *, ConnectionType, char *);
	void *userData;
};

struct iceAgentClient {
	IceAgent *iceAgent;
	unsigned short remotePort;
	unsigned short localPort;
	TunnelProtocols proto;
};

IOTC_PRIVATE const gchar *candidateTypeName[] = {"host", "srflx", "prflx", "relay"};
IOTC_PRIVATE const gchar *stateName[] = {"disconnected", "gathering", "connecting", "connected", "ready", "failed"};

#ifdef DEBUG
IOTC_PRIVATE void newSelectedPairCb(NiceAgent *agent, guint streamId, guint componentId, gchar *lFoundation, gchar *rFoundation, gpointer data) {
	printf("[DEBUG] Selected pair: %s %s for component %d\n", lFoundation, rFoundation, componentId);
}

IOTC_PRIVATE void stats(IceAgent *iceAgent) {
#if 0
	int i;
	printf("\033[32m[INFO]\n");
	printf("\t|Sent\t\t|Recv\nSocket\t|%15ld|%15ld\nAgent\t|%15ld|%15ld\n", sentSocket, recvSocket, sentIce, recvIce);
	printf("\033[0m\n");
	if(iceAgent != NULL) {
		printf("Active connections: ");
		for(i=0; i<ICE_MAX_CH; i++) {
			if(iceAgent->conns[i].sock != -1 || iceAgent->conns[i].connection != NULL)
				printf("%d ", i);
		}
	}
#endif
}
#endif

IOTC_PRIVATE int portMapInternal(IceAgent *iceAgent, unsigned short localPort, unsigned short remotePort,
		TunnelProtocols proto);

IOTC_PRIVATE gboolean timeoutCb(gpointer userData) {
	IceAgent *iceAgent = (IceAgent *)userData;
	char request[1];
	request[0] = P2P_TUNNEL_PING;
	time_t now = time(NULL);
	if(iceAgent->timeout == NULL)
		return G_SOURCE_REMOVE;
	double sec = difftime(now, *iceAgent->timeout);
	if(sec > ICE_TIMEOUT && iceAgent->onStatusChanged != NULL) {
		iceAgent->onStatusChanged(iceAgent->ctx, iceAgent, "timeout", iceAgent->userData, CONNECTION_NONE, "");
		return G_SOURCE_CONTINUE;
	} else if(sec > (ICE_TIMEOUT_INTERVAL/2)) {
		// send a ping only if there was no data exchanged
		// in last ICE_TIMEOUT_INTERVAL/2 seconds (note: integer division)
		if(iceSend(iceAgent, 0, 1, request) < 1) {
#ifdef DEBUG
			printf("[DEBUG] Cannot send ping...%f\n", sec);
#endif
		} else {
#ifdef DEBUG
			printf("[DEBUG] tunnel ping...%f\n", sec);
#endif
		}
	}
	return G_SOURCE_CONTINUE;
}

IOTC_PRIVATE void candidateGatheringDoneCb(NiceAgent *agent, guint streamId, gpointer data) {
	IceAgent *iceAgent = (IceAgent *)data;
	int offset = 0;
	char *localSdp;
	gchar *localUfrag = NULL;
	gchar *localPassword = NULL;
	gchar ipaddr[INET6_ADDRSTRLEN];
	GSList *cands = NULL, *item;

	if(!nice_agent_get_local_credentials(agent, streamId, &localUfrag, &localPassword)) {
		if(localUfrag) g_free(localUfrag);
		if(localPassword) g_free(localPassword);
#ifdef DEBUG
		printf("Error ICE agent cannot get local credentials\n");
#endif
		return;
	}

	if(!(cands = nice_agent_get_local_candidates(agent, streamId, 1))) {
		if(localUfrag) g_free(localUfrag);
		if(localPassword) g_free(localPassword);
		if(cands) g_slist_free_full(cands, (GDestroyNotify)&nice_candidate_free);
#ifdef DEBUG
		printf("Error ICE agent cannot get local candidates\n");
#endif
		return;
	}

	if(iceAgent->timeout == NULL) {
		iceAgent->timeout = (time_t *)malloc(sizeof(time_t));
#ifdef DEBUG
		if(iceAgent->timeout == NULL)
			printf("Malloc error: timeout\n");
#endif
		time(iceAgent->timeout);
		iceAgent->gsourceTimeout = g_timeout_add_seconds(ICE_TIMEOUT_INTERVAL, &timeoutCb, iceAgent);
	}
	if(iceAgent->onReady != NULL) {
		localSdp = (char *)malloc(1024);
#ifdef DEBUG
		if(localSdp == NULL)
			printf("Malloc error: localSdp\n");
#endif
		offset += snprintf(localSdp, 1024, "%s %s", localUfrag, localPassword);

		for(item = cands; item && offset < 1024; item = item->next) {
			NiceCandidate *cand = (NiceCandidate *)item->data;
			nice_address_to_string(&cand->addr, ipaddr);
			offset += snprintf(localSdp+offset, 1024-offset, " %s,%u,%s,%u,%s", cand->foundation, cand->priority, ipaddr, nice_address_get_port(&cand->addr), candidateTypeName[cand->type]);
		}

//		((void (*)(NiceAgent *, char *))data)(agent, localSdp);
		iceAgent->onReady(iceAgent->ctx, iceAgent, localSdp, iceAgent->userData);
		free(localSdp);
	}
	if(localUfrag) g_free(localUfrag);
	if(localPassword) g_free(localPassword);
	if(cands) g_slist_free_full(cands, (GDestroyNotify)&nice_candidate_free);
}

IOTC_PRIVATE bool isSameLan(NiceAddress local, NiceAddress remote) {
	if(nice_address_ip_version(&local) != 4
			|| nice_address_ip_version(&remote) != 4)
		return false;
	bool ret = false;
	int localAddr, remoteAddr;
	int addr, mask;
	localAddr = ((struct sockaddr_in *)&local.s)->sin_addr.s_addr;
	remoteAddr = ((struct sockaddr_in *)&remote.s)->sin_addr.s_addr;
#ifdef IFADDRS_NOT_SUPPORTED
	struct ifconf ifc;
	struct ifreq ifr[10];
	int i, ifc_num;

	int sock = socket(PF_INET, SOCK_DGRAM, 0);
	if(sock > 0) {
		ifc.ifc_len = sizeof(ifr);
		ifc.ifc_ifcu.ifcu_buf = (caddr_t)ifr;
		if(ioctl(sock, SIOCGIFCONF, &ifc) == 0) {
			ifc_num = ifc.ifc_len / sizeof(struct ifreq);
			for(i=0; i < ifc_num; ++i) {
				if(ifr[i].ifr_addr.sa_family != AF_INET)
					continue;
				if(ioctl(sock, SIOCGIFADDR, &ifr[i]) == 0) {
					addr = ((struct sockaddr_in *)(&ifr[i].ifr_addr))->sin_addr.s_addr;
					if(addr != localAddr)
						continue;
					if(ioctl(sock, SIOCGIFNETMASK, &ifr[i]) == 0) {
						mask = ((struct sockaddr_in *)(&ifr[i].ifr_netmask))->sin_addr.s_addr;
						if((localAddr & mask) == (remoteAddr & mask))
							ret = true;
						else
							ret = false;
						break;
					}
				}
			}
		}
		close(sock);
	}
#else
	struct ifaddrs *ifAddrStruct, *ifa;
	getifaddrs(&ifAddrStruct);

	for(ifa=ifAddrStruct; ifa!=NULL; ifa=ifa->ifa_next) {
		if(!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
			continue;
		addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
		if(addr != localAddr)
			continue;
		mask = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr.s_addr;
		if((localAddr & mask) == (remoteAddr & mask))
			ret = true;
		else
			ret = false;
		break;
	}
	if(ifAddrStruct!=NULL)
		freeifaddrs(ifAddrStruct);
#endif
	return ret;
}

IOTC_PRIVATE void componentStateChangedCb(NiceAgent *agent, guint streamId, guint componentId, guint state,
		gpointer data) {
	char remoteIp[INET6_ADDRSTRLEN];
	remoteIp[0] = '\0';
	IceAgent *iceAgent = (IceAgent *)data;
#ifdef DEBUG
//	printf("[DEBUG] State changed %d %d %s[%d]\n", streamId, componentId, stateName[state], state);
#endif
	ConnectionType connType;
	NiceCandidate *localCand, *remoteCand;
	if(!nice_agent_get_selected_pair(agent, streamId, componentId, &localCand, &remoteCand)) {
		connType = CONNECTION_NONE;
	} else if(localCand->type == NICE_CANDIDATE_TYPE_RELAYED
			|| remoteCand->type == NICE_CANDIDATE_TYPE_RELAYED) {
		connType = CONNECTION_RELAY;
		nice_address_to_string(&remoteCand->addr, remoteIp);
	} else if(localCand->type == NICE_CANDIDATE_TYPE_HOST
			&& remoteCand->type == NICE_CANDIDATE_TYPE_HOST
			&& isSameLan(localCand->addr, remoteCand->addr)) {
		connType = CONNECTION_LAN;
		nice_address_to_string(&remoteCand->addr, remoteIp);
	} else {
		connType = CONNECTION_P2P;
		nice_address_to_string(&remoteCand->addr, remoteIp);
	}

	if(iceAgent->onStatusChanged != NULL)
		iceAgent->onStatusChanged(iceAgent->ctx, iceAgent, stateName[state],
				iceAgent->userData, connType, remoteIp);
}

IOTC_PRIVATE void niceCanWriteCb(NiceAgent *agent, guint streamId, guint componentId, gpointer userData) {
	ConnectionInfo *conn = (ConnectionInfo *)userData;
	if(conn->unsentBytes > 0) {
		int sent = nice_agent_send(agent, 1, 1, conn->unsentBytes, conn->buffer+conn->sentBytes);
#ifdef DEBUG
		printf("Callback sent %d/%d\n", sent, conn->unsentBytes);
#endif
		conn->unsentBytes -= sent;
		conn->sentBytes += sent;
		// if there are no more data pending unlock all
		if(conn->unsentBytes == 0) {
			*(conn->pendingSend) = false;
			//Remove this callback
			g_signal_handler_disconnect(G_OBJECT(agent), conn->iceCanWriteSignalHandler);
			conn->iceCanWriteSignalHandler = 0;
//			g_signal_connect(G_OBJECT(agent), "reliable-transport-writable", G_CALLBACK(NULL), NULL);
		}
#ifdef DEBUG
		if(sent > 0)
			sentIce += sent;
	} else {
		printf("Error: Ice Agent can write callback called, but no data is pending\n");
#endif
	}
}

IOTC_PRIVATE void closeChannelAndSocket(ConnectionInfo *conn, bool sendToOtherAgent) {
	if(sendToOtherAgent && (conn->pendingSend == NULL || !*(conn->pendingSend))) {
		// Send command to other agent to close connection
		char *request = malloc(5);
#ifdef DEBUG
		if(request == NULL)
			printf("Malloc error: request\n");
#endif
		request[0] = 0;
		request[1] = 0;
		request[2] = 2;
		request[3] = P2P_TUNNEL_SHUT;
		request[4] = conn->channel;
		// TODO control send ok
		int sent = nice_agent_send(conn->agent, 1, 1, 5, request);
		free(request);
		if(sent != 5) {
#ifdef DEBUG
			printf("\033[31mFATAL agent send ko!\033[0m\n");
#endif
		}
	}

	// close this socket
	if(conn->sock != -1) {
		close(conn->sock);
		conn->sock = -1;
		if(conn->gsource > 0) {
			g_source_remove(conn->gsource);
			conn->gsource = 0;
		}
#ifdef DEBUG
	} else {
		printf("[DEBUG] closing socket that is not actually open\n");
#endif
	}
	if(conn->connection != NULL) {
		g_object_unref(conn->connection);
		conn->connection = NULL;
	}
#ifdef DEBUG
	sentIce += 5;
	printf("Socket read error: closing socket and deallocating recv callback\n");
	stats(NULL);
	printf("[DEBUG] Socket close: %s:%d \n- %s:%d\n",
			inet_ntoa(conn->srcAddr.sin_addr), ntohs(conn->srcAddr.sin_port),
			inet_ntoa(conn->dstAddr.sin_addr), ntohs(conn->dstAddr.sin_port));
#endif
}

IOTC_PRIVATE gboolean socketRecvCb(GObject *sourceObject, GAsyncResult *res, gpointer userData) {
	int readed;
	ConnectionInfo *conn = (ConnectionInfo *)userData;
	if(conn->pendingSend == NULL)
		return FALSE;
	if(*(conn->pendingSend)) {
#ifdef DEBUG
		printf("[DEBUG] I received more data, but I'm not ready to write...please slow down!\n");
#endif
		// TODO check if OK with NVR
		usleep(10000);
		return TRUE;
	}
	struct sockaddr_in addr;
	socklen_t slen = sizeof(struct sockaddr);
	if((readed = recvfrom(conn->sock, (conn->buffer)+3, BUFFER_LEN-3, MSG_DONTWAIT, (struct sockaddr *)&addr, &slen)) > 0) {
#ifdef DEBUG
		recvSocket += readed;
#endif
		conn->buffer[0] = conn->channel;
		conn->buffer[1] = (unsigned char)(readed >> 8);
		conn->buffer[2] = (unsigned char)readed;

		int sent;

		// TODO nice send should be unified in a single function
		sent = nice_agent_send(conn->agent, 1, 1, readed+3, conn->buffer);
		if(sent < 0) {
#ifdef DEBUG
			printf("Not sent, retry\n");
#endif
			*(conn->pendingSend) = true;
			conn->sentBytes = 0;
			conn->unsentBytes = readed+3;
			conn->iceCanWriteSignalHandler = g_signal_connect(G_OBJECT(conn->agent), "reliable-transport-writable", G_CALLBACK(niceCanWriteCb), conn);
			return TRUE;
		} else if(sent<(readed+3)) {
#ifdef DEBUG
			sentIce += sent;
			printf("Partially sent [%d/%d]...should resend %d bytes\n", sent, readed+3, readed+3-sent);
#endif
			*(conn->pendingSend) = true;
			conn->sentBytes = sent;
			conn->unsentBytes = readed-sent+3;
			conn->iceCanWriteSignalHandler = g_signal_connect(G_OBJECT(conn->agent), "reliable-transport-writable", G_CALLBACK(niceCanWriteCb), conn);
			return TRUE;
		}
#ifdef DEBUG
		sentIce += sent;
		printf("Nice sent [%d] [%ld]\n", readed+3, sentIce);
		stats(NULL);
#endif
		conn->unsentBytes = 0;
	} else if(readed == 0 || (readed == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) {
		// if it is RTSP close RTP channels too
		if(conn->proto == P2P_RTSP) {
			struct connectionList *l, *next;
			l = conn->rtpChList;
			conn->rtpChList = NULL;
			while(l != NULL) {
#ifdef DEBUG
				printf("Calling close channel on socket recv in\n");
#endif
				closeChannelAndSocket(l->value, true);
				next = l->next;
				l->next = NULL;
				free(l);
				l = next;
			}
		}
		// close channel and socket
#ifdef DEBUG
		printf("Calling close channel on socket recv\n");
#endif
		closeChannelAndSocket(conn, true);
		// remove event source! (and callback)
		return FALSE;
	}
	return TRUE;
}

IOTC_PRIVATE bool initSocket(ConnectionInfo *conn) {
	if(conn->proto == P2P_UDP) {
		conn->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if(conn->sock == -1) {
#ifdef DEBUG
			printf("Socket initialization failed: cannot create socket\n");
#endif
			return false;
		}
		if(bind(conn->sock, (struct sockaddr*)&(conn->srcAddr), sizeof(struct sockaddr)) == -1) {
#ifdef DEBUG
			printf("Socket initialization failed: cannot bind socket\n");
#endif
			close(conn->sock);
			conn->sock = -1;
			if(conn->connection != NULL) {
				g_object_unref(conn->connection);
				conn->connection = NULL;
			}
			return false;
		}
	} else if(conn->proto == P2P_TCP || conn->proto == P2P_RTSP) {
		conn->sock = socket(AF_INET, SOCK_STREAM, 0);
		if(conn->sock == -1) {
#ifdef DEBUG
			printf("Socket initialization failed: cannot create socket\n");
#endif
			return false;
		}

		int ret = connect(conn->sock, (struct sockaddr *)&(conn->dstAddr), sizeof(struct sockaddr));
		if(ret < 0) {
#ifdef DEBUG
			printf("Socket initialization failed: cannot connect to server socket\n");
#endif
			return false;
		}
	} else {
#ifdef DEBUG
		printf("Socket initialization failed: Unrecognized protocol");
#endif
		return false;
	}
	// Init listen callback
	GIOChannel* channel = g_io_channel_unix_new(conn->sock);
	// guint source...is id of source maybe it is useful to destroy source
	conn->gsource = g_io_add_watch(channel, G_IO_IN, (GIOFunc)socketRecvCb, conn);
//	g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, G_IO_IN, (GIOFunc)socketRecvCb, conn, exitCb);
	g_io_channel_unref(channel);
	return true;
}

IOTC_PRIVATE void niceRecvCb(NiceAgent *agent, guint streamId, guint componentId, guint len, gchar *buf, gpointer data) {
	char *packet;
	int ch, copyBytes = 0;
	ssize_t err, sent;
	if(len <= 0) {
#ifdef DEBUG
		printf("No data, but ice recv callback triggered\n");
#endif
		return;
	}
#ifdef DEBUG
	recvIce += len;
#endif
	IceAgent *iceAgent = (IceAgent *)data;
	ConnectionInfo *conns = iceAgent->conns;

	while(true) {
		// if readed bytes are same or more than packet size, this loop
		// has been restarting after a packet succesfully sent so
		// reset or update sent informations
		if(iceAgent->readedBytes == (iceAgent->packetSize + 3)) {
			iceAgent->readedBytes = 0;
			iceAgent->packetSize = 0;
#ifdef DEBUG
			printf("PACCHETTO COMPLETO SPEDITO, AZZERO!\n");
#endif
		} else if(iceAgent->readedBytes > (iceAgent->packetSize + 3)) {
#ifdef DEBUG
			printf("PACCHETTO COMPLETO SPEDITO, MA HO ANCORA DATI\n");
#endif
			memmove(iceAgent->recvBuffer, iceAgent->recvBuffer+iceAgent->packetSize+3,
					iceAgent->readedBytes-iceAgent->packetSize-3);
			iceAgent->readedBytes -= (iceAgent->packetSize + 3);
			iceAgent->packetSize = 0;
		}
		// if packet size is still unknown or I have no readed enough bytes read
		if(iceAgent->packetSize == 0 ||
				iceAgent->readedBytes < (iceAgent->packetSize + 3)) {
			if(len <= (BUFFER_LEN-iceAgent->readedBytes)) {
				copyBytes = len;
			} else {
				copyBytes = BUFFER_LEN-iceAgent->readedBytes;
			}
			memcpy(iceAgent->recvBuffer+iceAgent->readedBytes, buf, copyBytes);
			len -= copyBytes;
			buf += copyBytes;
			iceAgent->readedBytes += copyBytes;
		}

		// if I have readed enough bytes, parse packet size otherwise
		// wait for more data from ice channel
		if(iceAgent->readedBytes >= 3) {
			ch = iceAgent->recvBuffer[0];
			iceAgent->packetSize = ((unsigned char)iceAgent->recvBuffer[2]) + (((unsigned int)iceAgent->recvBuffer[1])<<8);
			// if packet size is invalid there is a parse error discard message
			if(iceAgent->packetSize > BUFFER_LEN || iceAgent->packetSize < 0) {
#ifdef DEBUG
				printf("Error: invalid packet size\n");
#endif
				iceAgent->readedBytes = 0;
				iceAgent->packetSize = 0;
				return;
			}
		} else {
#ifdef DEBUG
			printf("UN ALTRO GIRO...POCHI BYTES LETTI [%d]\n", iceAgent->readedBytes);
#endif
			return;
		}
		// if I have readed not enough bytes to complete packet wait for more data
		if(iceAgent->readedBytes < (iceAgent->packetSize + 3)) {
#ifdef DEBUG
			printf("UN ALTRO GIRO...PACCHETTO INCOMPLETO PER %d: %d/%d\n",
				iceAgent->recvBuffer[0], iceAgent->readedBytes, iceAgent->packetSize+3);
#endif
			return;
		}

		packet = iceAgent->recvBuffer+3;
#ifdef DEBUG
		printf("packet for %d channel of %d bytes\n", ch, iceAgent->packetSize);
#endif

		// new packet received, renew timeout for connection
		if(iceAgent->timeout != NULL)
			time(iceAgent->timeout);
		if(ch == 0) { // control channel
			int newCh;
			int action = packet[0];
			char request[1]; // used for pong
			switch(action) {
				case P2P_TUNNEL_MAP:
					if(iceAgent->packetSize<7) {
#ifdef DEBUG
						printf("Agent recv: not enough arguments to start a tunnel mapping\n");
#endif
						return;
					}
					newCh = (int)packet[1];
					if(newCh<=0 || ch>=ICE_MAX_CH) {
#ifdef DEBUG
						printf("Agent recv: tunnel mapping to invalid channel\n");
#endif
						return;
					}
					if(conns[newCh].sock != -1) {
#ifdef DEBUG
						printf("Agent recv: tunnel mapping to channel already open\n");
#endif
						return;
					}
					int srcPort = ((unsigned char)packet[3]) + (((unsigned int)packet[2])<<8);
					int dstPort = ((unsigned char)packet[5]) + (((unsigned int)packet[4])<<8);
					int proto = packet[6];
					// init dest sockaddr
					conns[newCh].dstAddr.sin_family = AF_INET;
					conns[newCh].dstAddr.sin_port = htons(dstPort);
					conns[newCh].dstAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); //htonl(INADDR_ANY);
					// init src sockaddr
					conns[newCh].srcAddr.sin_family = AF_INET;
					conns[newCh].srcAddr.sin_port = htons(srcPort);
					conns[newCh].srcAddr.sin_addr.s_addr = INADDR_ANY;//inet_addr("127.0.0.1");
					// init other parameters
					conns[newCh].channel = newCh;
					conns[newCh].proto = proto;
					conns[newCh].agent = agent;
					initSocket(&(conns[newCh]));
				break;
				case P2P_TUNNEL_SHUT:
#ifdef DEBUG
					printf("[DEBUG] Received tunnel shut\n");
#endif
					newCh = (int)packet[1];
					closeChannelAndSocket(&conns[newCh], false);
//					shutdown(conns[newCh].sock, SHUT_WR);
//					close(conns[newCh].sock);
//					conns[newCh].sock = -1;
				break;
				case P2P_TUNNEL_PING:
#ifdef DEBUG
					printf("[DEBUG] Received tunnel ping\n");
#endif
					request[0] = P2P_TUNNEL_PONG;
					if(iceSend(iceAgent, 0, 1, request) < 1) {
#ifdef DEBUG
						printf("[DEBUG] Cannot send pong...\n");
#endif
					} else {
#ifdef DEBUG
						printf("[DEBUG] tunnel pong...\n");
#endif
					}
				break;
				case P2P_TUNNEL_PONG:
#ifdef DEBUG
					printf("[DEBUG] Received tunnel pong\n");
#endif
				break;
				default:
#ifdef DEBUG
					printf("Agent recv invalid action\n");
#endif
				break;
			}
		} else if(ch>0 && ch<ICE_MAX_CH) { // one of communications channels
			if(conns[ch].sock == -1) {
				initSocket(&(conns[ch]));
			}
			sent = 0;
			// if it is RTSP do packet inspection
			if(conns[ch].proto == P2P_RTSP) {
#ifdef DEBUG
				printf("Received %d RTSP: %.*s", iceAgent->packetSize, iceAgent->packetSize, packet);
#endif
				// to permit strstr to not read over the end of packet (because it is not null
				// null terminated) replace last character with \0, so it can be consider
				// as a string
				char lastChar = packet[iceAgent->packetSize-1];
				packet[iceAgent->packetSize-1] = '\0';
				// when server send client and server port for RTP
				if(strstr(packet, "client_port=") != NULL && strstr(packet, "server_port=") != NULL) {
					char *rtpClientPort = strstr(packet, "client_port=");
					char *rtpServerPort = strstr(packet, "server_port=");
					rtpClientPort += 12;
					rtpServerPort += 12;
					int i=0, j=0;
					// just a check to control string has expected syntax
					while(rtpClientPort[i] != '\0' && rtpClientPort[i] != ';'
							&& rtpClientPort[i] != '-') {
						i++;
					}
					while(rtpServerPort[j] != '\0' && rtpServerPort[j] != ';'
							&& rtpServerPort[j] != '-') {
						j++;
					}
					if(i != 0 && j != 0) {
						int cliPort = atoi(rtpClientPort);
						int srvPort = atoi(rtpServerPort);
#ifdef DEBUG
						printf("Open new RTP connections on ports %d-%d %d-%d\n", cliPort, srvPort, cliPort+1, srvPort+1);
#endif
						// open portMap and save RTP channel for deallocation
						struct connectionList *rtpCh = (struct connectionList *)malloc(sizeof(struct connectionList));
#ifdef DEBUG
						if(rtpCh == NULL)
							printf("Malloc error: rtpCh\n");
#endif
						rtpCh->value = &conns[portMapInternal(iceAgent, cliPort, srvPort, P2P_UDP)];
						rtpCh->next = conns[ch].rtpChList;
						conns[ch].rtpChList = rtpCh;

						rtpCh  = (struct connectionList *)malloc(sizeof(struct connectionList));
#ifdef DEBUG
						if(rtpCh == NULL)
							printf("Malloc error: rtpCh2\n");
#endif
						rtpCh->value = &conns[portMapInternal(iceAgent, cliPort+1, srvPort+1, P2P_UDP)];
						rtpCh->next = conns[ch].rtpChList;
						conns[ch].rtpChList = rtpCh;
					}
				}
				// put character back
				packet[iceAgent->packetSize-1] = lastChar;
			}

			while(sent < iceAgent->packetSize) {
				err = sendto(conns[ch].sock, packet+sent, iceAgent->packetSize-sent, 0, (struct sockaddr *)&(conns[ch].dstAddr), sizeof(struct sockaddr));
				if(err == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
#ifdef DEBUG
					printf("Socket send error: EAGAIN\n");
#endif
				} else if(err == -1) {
#ifdef DEBUG
					printf("Socket send error[%d] on channel %d: closing socket\n", errno, ch);
					stats(NULL);
#endif
					// if it is RTSP close RTP channels too
					if(conns[ch].proto == P2P_RTSP) {
						struct connectionList *l, *next;
						l = conns[ch].rtpChList;
						conns[ch].rtpChList = NULL;
						while(l != NULL) {
#ifdef DEBUG
							printf("Calling close channel on nice recv\n");
#endif
							closeChannelAndSocket(l->value, true);
							next = l->next;
							l->next = NULL;
							free(l);
							l = next;
						}
					}

					closeChannelAndSocket(&conns[ch], true);
/*					close(conns[ch].sock);
#ifdef DEBUG
					printf("[DEBUG] SOCKET close1: %s:%d - %s:%d\n",
							inet_ntoa(conns[ch].srcAddr.sin_addr), ntohs(conns[ch].srcAddr.sin_port),
							inet_ntoa(conns[ch].dstAddr.sin_addr), ntohs(conns[ch].dstAddr.sin_port));
#endif
					conns[ch].sock = -1;
					if(conns[ch].connection != NULL) {
						g_object_unref(conns[ch].connection);
						conns[ch].connection = NULL;
					}
*/
					//return;
					break;
				} else {
					sent += err;
#ifdef DEBUG
					sentSocket += err;
					if(sent < iceAgent->packetSize)
						printf("Socket send error: partially sent [%ld] [%ld]\n", err, sentSocket);
					else
						printf("Socket send: sent [%ld] [%ld]\n", err, sentSocket);
#endif
				}
			}
		} else { // invalid channel
#ifdef DEBUG
			printf("Agent recv channel invalid\n");
#endif
		}
	}
}

IceAgent *iceNew(IotcCtx *ctx, GMainLoop *gloop,
		const char *host, int port, const char *turnUser, const char *turnPassword,
		void (*onReady)(IotcCtx *ctx, IceAgent *, char *, void *),
		void (*onStatusChanged)(IotcCtx *ctx, IceAgent *, const char *, void *, ConnectionType, char *),
		void *userData) {
	int i;
	int streamId;
	ConnectionInfo *conns;
	// Initialize agent
	NiceAgent *agent = nice_agent_new_reliable(g_main_loop_get_context(gloop), NICE_COMPATIBILITY_RFC5245);
	if(!agent) {
#ifdef DEBUG
		printf("Cannot create ICE agent\n");
#endif
		return NULL;
	}

	// Set agent parameters
	g_object_set(G_OBJECT(agent), "stun-server", host, NULL);
	g_object_set(G_OBJECT(agent), "stun-server-port", port, NULL);
	g_object_set(G_OBJECT(agent), "controlling-mode", 0, NULL);

#ifdef DEBUG
	// Check upnp is on
	gboolean upnp;
	g_object_set(G_OBJECT(agent), "upnp", true, NULL);
	g_object_get(G_OBJECT(agent), "upnp", &upnp, NULL);
	printf("UPNP enabled: %d\n", upnp);

	// Enable Selected pair callback just to check what is happening inside agent
	g_signal_connect(G_OBJECT(agent), "new-selected-pair", G_CALLBACK(newSelectedPairCb), NULL);
#endif

	IceAgent *iceAgent = (IceAgent *)malloc(sizeof(IceAgent));

#ifdef DEBUG
	if(iceAgent == NULL)
		printf("Malloc error: iceAgent\n");
#endif
	iceAgent->agent = agent;
	iceAgent->ctx = ctx;
	iceAgent->onReady = onReady;
	iceAgent->onStatusChanged = onStatusChanged;
	iceAgent->userData = userData;
	iceAgent->pendingSend = false;
	iceAgent->recvBuffer = (char *)malloc(BUFFER_LEN);
#ifdef DEBUG
	if(iceAgent->recvBuffer == NULL)
		printf("Malloc error: iceAgent->recvBuffer\n");
#endif
	iceAgent->readedBytes = 0;
	iceAgent->packetSize = 0;
	iceAgent->timeout = NULL;
	iceAgent->socketServiceList = NULL;
	// ConnectionInfo intiliazation
	conns = (ConnectionInfo *)malloc(sizeof(ConnectionInfo)*ICE_MAX_CH);
#ifdef DEBUG
	if(conns == NULL)
		printf("Malloc error: conns\n");
#endif

	for(i=0; i<ICE_MAX_CH; i++) {

		conns[i].sock = -1;
		conns[i].gsource = 0;
		conns[i].connection = NULL;
		conns[i].pendingSend = &(iceAgent->pendingSend);
		conns[i].iceCanWriteSignalHandler = 0;
		conns[i].buffer = (char *)malloc(BUFFER_LEN);
#ifdef DEBUG
		if(conns[i].buffer == NULL)
			printf("Malloc error: conns[i].buffer\n");
#endif
		conns[i].unsentBytes = 0;
		conns[i].rtpChList = NULL;
	}
	iceAgent->conns = conns;

	// Set callback on finish gathering candidates
	g_signal_connect(G_OBJECT(agent), "candidate-gathering-done", G_CALLBACK(candidateGatheringDoneCb), iceAgent);
	// Set callback on connection state change (It's interesting just when is READY)
	g_signal_connect(G_OBJECT(agent), "component-state-changed", G_CALLBACK(componentStateChangedCb), iceAgent);

	if(!(streamId = nice_agent_add_stream(agent, 1))) {
#ifdef DEBUG
		printf("Cannot add streams to ICE agent\n");
#endif
		g_object_unref(agent);
		return NULL;
	}

	// For each component add callback for message receive. Now we have just 1 component...
	if(!nice_agent_attach_recv(agent, streamId, 1, g_main_loop_get_context(gloop), niceRecvCb, iceAgent)) {
#ifdef DEBUG
		printf("Cannot attach receive function to ICE agent\n");
#endif
		g_object_unref(agent);
		return NULL;
	}

	// For each component add turn info needed. Now we have just 1 component...
	if(!nice_agent_set_relay_info(agent, streamId, 1, host, port, turnUser, turnPassword, NICE_RELAY_TYPE_TURN_UDP)) {
#ifdef DEBUG
		printf("Invalid turn address for ICE agent\n");
#endif
		g_object_unref(agent);
		return NULL;
	}

	// Start gather candidates. It is an async call, but local candidates are found immediatly or
	// an error occurred
	if(!nice_agent_gather_candidates(agent, streamId)) {
#ifdef DEBUG
		printf("ICE agent cannot gather candidates\n");
#endif
		g_object_unref(agent);
		return NULL;
	}
#ifdef DEBUG
	printf("ICE Agent initialized on stream[%d]!\n", streamId);
#endif

	return iceAgent;
}

bool iceSetRemoteSdp(IceAgent *iceAgent, const char *remoteSdp) {
	int i, j;
	GSList *remoteCandidates = NULL;
	gchar **sdpArgv = NULL;
	const gchar *ufrag = NULL;
	const gchar *password = NULL;
	NiceAgent *agent = iceAgent->agent;


	if(!(sdpArgv = g_strsplit_set(remoteSdp, " \t\n", 0))) {
#ifdef DEBUG
		printf("ICE agent cannot get remote candidates\n");
#endif
		return false;
	}


	for(i=0; sdpArgv[i]; i++) {

		if(strlen(sdpArgv[i]) == 0)
			continue;

		if(!ufrag) {

			ufrag = sdpArgv[i];
		} else if(!password) {

			password = sdpArgv[i];
		} else {

			NiceCandidate *cand = NULL;
			NiceCandidateType type;
			gchar **tokens = g_strsplit(sdpArgv[i], ",", 5);

			for(j=0; tokens[j]; j++);
			if(j != 5) {
				g_strfreev(tokens);
#ifdef DEBUG
				printf("warning: ICE agent cannot split remote candidate: %s\n", sdpArgv[i]);
#endif
				//return false;
				break;
			}

			for(j=0; j<G_N_ELEMENTS(candidateTypeName); j++) {
				if(strcmp(tokens[4], candidateTypeName[j]) == 0) {
					type = j;
					break;
				}
			}

			if(j==G_N_ELEMENTS(candidateTypeName)) {
				g_strfreev(tokens);
#ifdef DEBUG
				printf("ICE agent cannot get remote candidate type\n");
#endif
				return false;
			}

			cand = nice_candidate_new(type);
			cand->component_id = 1;
			cand->stream_id = 1;
			cand->transport = NICE_CANDIDATE_TRANSPORT_UDP;
			strncpy(cand->foundation, tokens[0], NICE_CANDIDATE_MAX_FOUNDATION);
			cand->foundation[NICE_CANDIDATE_MAX_FOUNDATION - 1] = 0;
			cand->priority = atoi(tokens[1]);
#ifdef DEBUG
			printf("Foundation %s, priority %d\n", cand->foundation, cand->priority);
#endif
			if(!nice_address_set_from_string(&cand->addr, tokens[2])) {
				g_strfreev(tokens);
				nice_candidate_free(cand);
#ifdef DEBUG
				printf("ICE agent cannot get remote candidate address\n");
#endif
				return false;
			}
			nice_address_set_port(&cand->addr, atoi(tokens[3]));
			g_strfreev(tokens);

			remoteCandidates = g_slist_prepend(remoteCandidates, cand);
		}
	}
	if(!ufrag || !password || !remoteCandidates ||
		(!nice_agent_set_remote_credentials(agent, 1, ufrag, password)) ||
		(nice_agent_set_remote_candidates(agent, 1, 1, remoteCandidates) < 1)) {
		if(sdpArgv) g_strfreev(sdpArgv);
		if(remoteCandidates) g_slist_free_full(remoteCandidates, (GDestroyNotify)&nice_candidate_free);
#ifdef DEBUG
		printf("ICE agent cannot set remote candidates\n");
#endif
		return false;
	}
	if(sdpArgv) g_strfreev(sdpArgv);
	if(remoteCandidates) g_slist_free_full(remoteCandidates, (GDestroyNotify)&nice_candidate_free);
	return true;
}

int iceSend(IceAgent *iceAgent, char channel, int msgLen, char *msg) {
	int i;
	char buf[msgLen+3];
	for(i=0; i<msgLen; i++)
		buf[i+3] = msg[i];
	buf[0] = channel;
	buf[1] = (unsigned char)(msgLen >> 8);
	buf[2] = (unsigned char)msgLen;
#ifdef DEBUG
	sentIce += 3;
#endif
	// TODO stop when cannot send all data
	if(!iceAgent->pendingSend)
		return (nice_agent_send(iceAgent->agent, 1, 1, msgLen+3, buf) - 1);
	return 0;
}

gboolean socketListenCb(GSocketService *service, GSocketConnection *connection, GObject *sourceObject, gpointer userData) {
#ifdef DEBUG
	printf("Socket server new incoming connection\n");
#endif
	struct iceAgentClient *iac = (struct iceAgentClient *)userData;
	IceAgent *iceAgent = iac->iceAgent;
	int i;
	// find first free channel
	for(i=1; i<ICE_MAX_CH; i++)
		if(iceAgent->conns[i].sock == -1)
			break;
	// no free channel found
	if(i == ICE_MAX_CH) {
#ifdef DEBUG
		printf("ICE client cannot find a free channel\n");
#endif
		return false;
	}
	char *request = malloc(7);
#ifdef DEBUG
	if(request == NULL)
		printf("Malloc error: request2\n");
#endif
	request[0] = P2P_TUNNEL_MAP;
	request[1] = i;
	request[2] = (unsigned char)(iac->localPort >> 8);
	request[3] = (unsigned char)iac->localPort;
	request[4] = (unsigned char)(iac->remotePort >> 8);
	request[5] = (unsigned char)iac->remotePort;
	request[6] = iac->proto;
	if(iceSend(iceAgent, 0, 7, request) < 7) {
#ifdef DEBUG
		printf("ICE client cannot require map on device\n");
#endif
		free(request);
		return FALSE;
	}
	free(request);
	//source addr is useless...
	iceAgent->conns[i].srcAddr.sin_family = AF_INET;
	iceAgent->conns[i].srcAddr.sin_port = htons(iac->localPort);
	iceAgent->conns[i].srcAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	// initialize connected socket
	iceAgent->conns[i].dstAddr.sin_family = AF_INET;
	iceAgent->conns[i].dstAddr.sin_port = htons(iac->remotePort);
	iceAgent->conns[i].dstAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); //htonl(INADDR_ANY);
	iceAgent->conns[i].sock = g_socket_get_fd(g_socket_connection_get_socket(connection));
	iceAgent->conns[i].channel = i;
	iceAgent->conns[i].proto = iac->proto;
	iceAgent->conns[i].agent = iac->iceAgent->agent;
	g_object_ref(connection);
	iceAgent->conns[i].connection = connection;

	// Init listen callback
	GIOChannel* channel = g_io_channel_unix_new(iceAgent->conns[i].sock);
	iceAgent->conns[i].gsource = g_io_add_watch(channel, G_IO_IN, (GIOFunc)socketRecvCb,
			&(iceAgent->conns[i]));
	g_io_channel_unref(channel);

	return TRUE;
}

// returns -1 if port map has not be set up, 0 or a postive integer otherwise (for udp the channel number)
IOTC_PRIVATE int portMapInternal(IceAgent *iceAgent, unsigned short localPort, unsigned short remotePort,
		TunnelProtocols proto) {
	if(proto == P2P_UDP) {
		int i;
		// find first free channel
		for(i=1; i<ICE_MAX_CH; i++)
			if(iceAgent->conns[i].sock == -1)
				break;
		// no free channel found
		if(i == ICE_MAX_CH) {
#ifdef DEBUG
			printf("ICE client cannot find a free channel\n");
#endif
			return -1;
		}
		char *request = malloc(7);
#ifdef DEBUG
		if(request == NULL)
			printf("Malloc error: request3\n");
#endif
		request[0] = P2P_TUNNEL_MAP;
		request[1] = i;
		request[2] = (unsigned char)(localPort >> 8);
		request[3] = (unsigned char)localPort;
		request[4] = (unsigned char)(remotePort >> 8);
		request[5] = (unsigned char)remotePort;
		request[6] = proto;
		if(iceSend(iceAgent, 0, 7, request) < 7) {
#ifdef DEBUG
			printf("ICE client cannot require map on device\n");
#endif
			free(request);
			return -1;
		}
		free(request);

		// init src sockaddr
		iceAgent->conns[i].srcAddr.sin_family = AF_INET;
		iceAgent->conns[i].srcAddr.sin_port = htons(remotePort);
		iceAgent->conns[i].srcAddr.sin_addr.s_addr = INADDR_ANY;//inet_addr("127.0.0.1");

		// initialize connected socket
		iceAgent->conns[i].sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);;
		if(iceAgent->conns[i].sock == -1) {
#ifdef DEBUG
			printf("Socket initialization failed: cannot create socket\n");
#endif
			return -1;
		}
		if(bind(iceAgent->conns[i].sock, (struct sockaddr*)&(iceAgent->conns[i].srcAddr), sizeof(struct sockaddr)) == -1) {
#ifdef DEBUG
			printf("Socket initialization failed: cannot bind socket\n");
#endif
			close(iceAgent->conns[i].sock);
			iceAgent->conns[i].sock = -1;
			if(iceAgent->conns[i].connection != NULL) {
				g_object_unref(iceAgent->conns[i].connection);
				iceAgent->conns[i].connection = NULL;
			}
			return -1;
		}

		iceAgent->conns[i].dstAddr.sin_family = AF_INET;
		iceAgent->conns[i].dstAddr.sin_port = htons(localPort);
		iceAgent->conns[i].dstAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
		iceAgent->conns[i].channel = i;
		iceAgent->conns[i].proto = proto;
		iceAgent->conns[i].agent = iceAgent->agent;
		iceAgent->conns[i].connection = NULL;

		// Init listen callback
		GIOChannel* channel = g_io_channel_unix_new(iceAgent->conns[i].sock);
		iceAgent->conns[i].gsource = g_io_add_watch(channel, G_IO_IN, (GIOFunc)socketRecvCb,
				&(iceAgent->conns[i]));
		g_io_channel_unref(channel);

		return i;
	} else if(proto == P2P_TCP || proto == P2P_RTSP) {
		GSocketService *service;
		GError *error = NULL;
		gboolean ret;
		service = g_socket_service_new();
		// do not use g_socket_listener_add_inet_port because cannot bind to "any" interface
		//GInetAddress *address = g_inet_address_new_from_string("127.0.0.1");
		GInetAddress *address = g_inet_address_new_from_string("0.0.0.0");
		GSocketAddress *socketAddress = g_inet_socket_address_new(address, localPort);
		ret = g_socket_listener_add_address(G_SOCKET_LISTENER(service), socketAddress,
				G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, NULL, NULL, &error);
//		ret = g_socket_listener_add_inet_port(G_SOCKET_LISTENER(service), localPort, NULL, &error);
		g_object_unref(socketAddress);
		g_object_unref(address);
		if(!ret && error != NULL) {
#ifdef DEBUG
			printf("ICE client cannot allocate socket\n");
#endif
			g_clear_error(&error);
			g_object_unref(service);
			return -1;
		}
		struct iceAgentClient *iac = (struct iceAgentClient *)malloc(sizeof(struct iceAgentClient));
#ifdef DEBUG
		if(iac == NULL)
			printf("Malloc error: iac\n");
#endif
		iac->iceAgent = iceAgent;
		iac->remotePort = remotePort;
		iac->localPort = localPort;
		iac->proto = proto;

		struct socketServiceList *ssl =
				(struct socketServiceList *)malloc(sizeof(struct socketServiceList));
		ssl->service = service;
		ssl->iac = iac;
		ssl->next = iceAgent->socketServiceList;
		iceAgent->socketServiceList = ssl;

		g_signal_connect(G_OBJECT(service), "incoming", G_CALLBACK(socketListenCb), iac);
		g_socket_service_start(service);
		return 0;
	}
	return -1;
}

bool icePortMap(IceAgent *iceAgent, unsigned short localPort, unsigned short remotePort,
		TunnelProtocols proto) {
	return portMapInternal(iceAgent, localPort, remotePort, proto) >= 0;
}

void iceStop(IceAgent *iceAgent) {
	int i;
	// Remove all listening sockets
	struct socketServiceList *ssl = iceAgent->socketServiceList;
	iceAgent->socketServiceList = NULL;
	while(ssl != NULL) {
		struct socketServiceList *elem = ssl;
		ssl = ssl->next;
		g_socket_service_stop(elem->service);
		g_object_unref(elem->service);
		free(elem->iac);
		free(elem);
	}
	ConnectionInfo *conns = iceAgent->conns;
	if(conns != NULL) {
		// Remove all connected sockets because I'm closing agent
		for(i=0; i<ICE_MAX_CH; i++) {
			if(conns[i].sock != -1) {
				close(conns[i].sock);
				conns[i].sock = -1;
				g_source_remove(conns[i].gsource);
			}
			if(conns[i].connection != NULL) {
				g_object_unref(conns[i].connection);
				conns[i].connection = NULL;
			}
			if(conns[i].buffer != NULL) {
				free(conns[i].buffer);
				conns[i].buffer = NULL;
			}
		}
		free(conns);
		iceAgent->conns = NULL;
	}
	if(iceAgent->recvBuffer != NULL) {
		free(iceAgent->recvBuffer);
		iceAgent->recvBuffer = NULL;
	}
	if(iceAgent->timeout != NULL) {
		free(iceAgent->timeout);
		iceAgent->timeout = NULL;
	}
	if(NICE_IS_AGENT(iceAgent->agent)) {
		nice_agent_remove_stream(iceAgent->agent, 1);
		g_object_unref(iceAgent->agent);
		iceAgent->agent = NULL;
	}
	g_source_remove(iceAgent->gsourceTimeout); // remove timeout
}

void iceFree(IceAgent *iceAgent) {
	iceStop(iceAgent);
/*	int i;
	// Remove all listening sockets
	struct socketServiceList *ssl = iceAgent->socketServiceList;
	iceAgent->socketServiceList = NULL;
	while(ssl != NULL) {
		struct socketServiceList *elem = ssl;
		ssl = ssl->next;
		g_socket_service_stop(elem->service);
		g_object_unref(elem->service);
		free(elem->iac);
		free(elem);
	}
	ConnectionInfo *conns = iceAgent->conns;
	if(conns != NULL) {
		// Remove all connected sockets because I'm closing agent
		for(i=0; i<ICE_MAX_CH; i++) {
			if(conns[i].sock != -1) {
				close(conns[i].sock);
				conns[i].sock = -1;
				g_source_remove(conns[i].gsource);
			}
			if(conns[i].connection != NULL) {
				g_object_unref(conns[i].connection);
				conns[i].connection = NULL;
			}
			if(conns[i].buffer != NULL) {
				free(conns[i].buffer);
				conns[i].buffer = NULL;
			}
		}
		free(conns);
	}
	if(iceAgent->recvBuffer != NULL) {
		free(iceAgent->recvBuffer);
		iceAgent->recvBuffer = NULL;
	}
	if(iceAgent->timeout != NULL) {
		free(iceAgent->timeout);
		iceAgent->timeout = NULL;
	}
	if(NICE_IS_AGENT(iceAgent->agent)) {
		nice_agent_remove_stream(iceAgent->agent, 1);
		g_object_unref(iceAgent->agent);
		iceAgent->agent = NULL;
	}
	g_source_remove(iceAgent->gsourceTimeout); // remove timeout */
	free(iceAgent);
}
