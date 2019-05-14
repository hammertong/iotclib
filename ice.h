/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 * 
 * [any text provided by the client]
 *
 */

/**
 * @file ice.h
 * @author Matteo Di Leo <matteo.dileo@csp.it>
 * @date 19/02/2015
 * @brief Urmet IoT ICE implementation
 *
 * The Urmet IoT ICE implementation
 * Here are placed all the functions used to initialize and manage
 * ICE connection to a peer.
 * ICE implementation is based on libnice and uses upnp support enabled by gupnp.
 */

#ifndef __ICE_H__
#define __ICE_H__

#include <errno.h>
#include <stdbool.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nice/agent.h>

/**
 * @brief The Agent used for a specific ICE connection.
 */
typedef struct iceAgent IceAgent;

/**
 * @brief List of actions that client can request to device
 */
typedef enum {
	P2P_TUNNEL_MAP,		/**< Request a port mapping */
	P2P_TUNNEL_SHUT,	/**< Request socket close */
	P2P_TUNNEL_PING,	/**< Ping tunnel to check connection */
	P2P_TUNNEL_FREE,	/**< Request socket close and deallocation */
	P2P_TUNNEL_PONG,	/**< Response to a ping request */
} p2pActions;

/**
 * @brief Create an agent for ICE connection
 *
 * Create an agent for ICE connection. It uses a GMainLoop for all callbacks, so gloop
 * must be initialized and should be already running. Same instance of loop can manage
 * multiple agents.
 * @param ctx The context passed back to callbacks
 * @param gloop The Gnome Main Loop that agent uses to invoke callbacks and for networking operations
 * @param host The IP of host where stun/turn service is
 * @param port The port of stun/turn service (default 3478)
 * @param turnUser The username used for authenticating on turn service
 *	(can be NULL if no authentication required)
 * @param turnPassword The password for authenticating on turn service
 *	(can be NULL if no authentication required)
 * @param onReady The callback called when agent is ready to receive connections.
 *	The callback is invoked with local SDP as input parameter
 * @param onStatusChanged The callback called when agent change its connection status,
 *	the status is passed as a string; refer to array stateName for possible values
 * @param userData data passed back to callbacks
 * @return A pointer to a IceAgent correctly initialized or NULL if an error occurred
 */
IceAgent *iceNew(IotcCtx *ctx, GMainLoop *gloop,
		const char *host, int port, const char *turnUser, const char *turnPassword,
		void (*onReady)(IotcCtx *ctx, IceAgent *, char *, void *),
		void (*onStatusChanged)(IotcCtx *ctx, IceAgent *, const char *, void *, ConnectionType, char *),
		void *userData);

/**
 * @brief Set the remote SDP to ICE agent
 *
 * Set the remote SDP to ICE agent for connection and try to connect to remote agent.
 * Connection is possible even if local agent is still gathering candidates.
 * @param agent An ICE agent already initialized using iceNew()
 * @param remoteSdp The string generate by the remote agent that contains ufrag, password and
 *	all candidates available for connection
 * @return A boolean value which is true if set has been done, false otherwise.
 * @warning Connection to remote host is not synchronous and may also fail,
 *	so a callback is invoked to communicate status of connection. This callback is
 *	not available with this library
 */
bool iceSetRemoteSdp(IceAgent *iceAgent, const char *remoteSdp);

/**
 * @brief Send a message to the other peer on ice connection
 *
 * Send a message to the other peer using ice connection
 * @param iceAgent The agent used for ice connection to the peer
 * @param channel The channel where send data
 * @param msgLen The length of the message to send
 * @param msg The message to send
 * @return The number of bytes sent
 */
int iceSend(IceAgent *iceAgent, char channel, int msgLen, char *msg);

/**
 * @brief Require a port mapping
 *
 * @param iceAgent The agent used for ice connection to the device
 * @param localPort The port on localhost (client) on which client will listen for incoming connections
 * @param remotePort The port on the device where the desired service is listening
 * @param proto The protocol that will be used between final client and final server:
 *		this parameter accept Transport layer protocols (UDP/TCP) or Application layer (RTSP)
 * @return A boolean value, true if port mapping is correctly initialized, false otherwise
 * @see TunnelProtocols
 */
bool icePortMap(IceAgent *iceAgent, unsigned short localPort, unsigned short remotePort,
		TunnelProtocols proto);

/**
 * @brief Stop all IceAgent operations
 *
 * Stop the IceAgent and close all open sockets
 * @param agent The agent to stop
 */
void iceStop(IceAgent *agent);

/**
 * @brief Deallocate IceAgent structure
 *
 * Deallocate IceAgent structure and eventually stops all pending communications
 * @param agent The agent that should not be used anymore
 */
void iceFree(IceAgent *agent);

#endif
