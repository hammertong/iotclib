/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 * 
 * [any text provided by the client]
 * 
 */

/**
 * @file library.h
 * @author Matteo Di Leo <matteo.dileo@csp.it>
 * @date 11/02/2015
 * @brief Urmet IoT library implementation
 *
 * The Urmet IoT library implementation. Contains all the main functions to start ICE service
 * and get device ready to receive connection.
 */

#ifndef __LIBRARY_H__
#define __LIBRARY_H__

#define IOTC_PRIVATE static

#include <stdio.h>      
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <stdbool.h>

//#ifndef IOTC_CLIENT
#include "mqtt.h"
//#endif

#ifndef IFADDRS_NOT_SUPPORTED
#include <ifaddrs.h>
#endif

#ifndef SERVER_NAME
#define SERVER_NAME "www.cloud.urmet.com"
#endif

#ifndef WEBSERVICE_ENDPOINT
#define WEBSERVICE_ENDPOINT "https://www.cloud.urmet.com"
#endif

#ifndef WEBSERVICE_AUTHFORM
#define WEBSERVICE_AUTHFORM "httpd_username=dileo&httpd_password=dileo"
#endif

#ifndef ICE_SERVER
#define ICE_SERVER "35.195.29.62"
#endif

#ifndef ICE_SERVER_USER
#define ICE_SERVER_USER "admin"
#endif

#ifndef ICE_SERVER_PASS
#define ICE_SERVER_PASS "iotc$urm_2016"
#endif

/**
 * @brief The context used for all connection operations
 *
 * @see iotcInitClient()
 * @see iotcInitDevice()
 */
typedef struct iotcCtx IotcCtx;

/**
 * @brief The Agent used for a specific connection.
 */
typedef struct iotcAgent IotcAgent;

/**
 * @brief Protocols known by tunnel system
 */
typedef enum {
        P2P_UDP,
        P2P_TCP,
        P2P_RTSP,
} TunnelProtocols;

/**
 * @brief Connection type established between peers
 */
typedef enum {
	CONNECTION_LAN,
	CONNECTION_P2P,
	CONNECTION_RELAY,	/**< At least one agent is using a relay channel */
	CONNECTION_NONE,	/**< Connection has not yet been established or has been closed */
} ConnectionType;

/**
 * @brief Status of IotcAgent
 */
typedef enum {
	STATUS_DISCONNECTED,	/**< Agent has just been created and not connected yet */
	STATUS_GATHERING,	/**< Agent is gathering its network parameters */
	STATUS_CONNECTING,	/**< Agent is connecting to remote end point */
	STATUS_CONNECTED,	/**< Agent is connected to remote end point but is still working */
	STATUS_READY,		/**< Agent is connected and ready to receive and send data */
	STATUS_FAILED,		/**< Agent connection failed */
	STATUS_TIMEOUT,		/**< Agent cannot comunicate to remote end point for too much time */
} AgentStatus;

/**
 * @brief The struct used to list and describe discovered devices
 *
 * @see lanDiscovery()
 */
struct deviceDiscoveredList {
	char *uuid;				/**< The uuid used to recognize the device */
	char *urnDeviceType;			/**< The type of the device discovered */
	char *ip;				/**< The IP declared by the device */
	struct deviceDiscoveredList *next;	/**< A pointer to next element of the list */
};

//#ifndef IOTC_CLIENT
/**
 * @brief Start device. This function lock until device stop working.
 *
 * @param uid The uid of this device
 * @param basePath The path where configuration files are written
 * 	(no more than 20KB should be written and no more than 10 files)
 * 	the path must terminate with character '/'
 * @note User that launches application must have read/write permissions
 * 	on the specified path
 */
int iotcInitDevice(char *uid, char *basePath);
//#endif // IOTC_CLIENT

/**
 * @brief Create an IotcCtx and start client loop
 *
 * Use iotcDeinit() to destroy the created context
 *
 * @return A pointer to the IotcCtx created or NULL if an error occurred
 * @see iotcDeinit()
 */
IotcCtx *iotcInitClient();

/**
 * @brief Destroy an IotcCtx created by iotcInitClient()
 *
 * This function is used to stop running operations and deallocate the context.
 * All IotcAgents created using this context must be disconnected before invoking this function,
 * otherwise some agents may not be deallocated correctly.
 * After iotcDeinit() the IotcCtx passed as parameter cannot be used anymore.
 *
 * @param iotcCtx The context to deallocate
 * @note This is a blocking operation that could lasts some seconds
 */
void iotcDeinit(IotcCtx *iotcCtx);

/**
 * @brief Connect to a device
 *
 * This function is used by client to connect to a device.
 * Every agent created using this function should be disconnected, when it is no more needed,
 * using iotcDisconnect() function.
 *
 * @see enum ConnectionType
 * @see enum AgentStatus
 * @param ctx The IotcCtx created using iotcInitClient()
 * @param uid The uid of the device to connect to
 * @param serverIp The ip of a connection server obtained from main server
 * @param serverUsername Username used for turn authentication
 * @param serverPassword Password used for turn authentication
 * @param getRemoteSdp A callback used by the client to set device sdp. The sdp should be
 * 		acquired asking to main server via HTTPs. Params are:
 *	- uid The uid of the device
 *	- localSdp The sdp of this client
 *	- userData The user data provided as parameter in this funtion
 *	- return The sdp of the device. The library will free this value when it is no more needed
 * @param connectionStatusCb A callbacke invoked when IcaAgent status changes. Params are:
 *	- iotcAgent The agent used for this connection
 *	- status The new status of the agent
 *	- connType The type of connection between peers
 *	- remoteIp The IP of the remote endpoint if known (can be an empty string, but not NULL)
 *	- userData The user data provided as parameter in this funtion
 * @param userData A pointer to data passed back to callbacks
 * @return The IotcAgent of the connection or NULL if an error occurred
 * @see iotcDisconnect()
 */
IotcAgent *iotcConnect(IotcCtx *ctx, const char *uid,
		const char *serverIp, const char *serverUsername, const char *serverPassword,
		const char *(*getRemoteSdp)(char *uid, char *localSdp, void *userData),
		void (*connectionStatusCb)(IotcAgent *iotcAgent, const char *status,
				ConnectionType connType, char *remoteIp, void *userData),
		void *userData);

/**
 * @brief Disconnect an IotcAgent
 *
 * This function is used to disconnect an IotcAgent created using iotcConnect().
 * Disconnect must be called for every agent created when it is not needed anymore,
 * even if connection has not been correctly initialized.
 * Disconnect can be called even if connection has not been already completed.
 * After iotcDisconnect() the IotcAgent passed as parameter cannot be used anymore.
 * Every IotcAgent should be disconnected before invoke iotcDeinit()
 *
 * @param iotcAgent The agent to disconnect and destroy
 * @see iotcConnect()
 */
void iotcDisconnect(IotcAgent *iotcAgent);

/**
 * @brief Require a port mapping
 *
 * Require a port mapping, this function should be used by clients to require a port mapping
 * to the device.
 * @param iotcAgent The agent used for connection to the device
 * @param localPort The port on localhost (client) on which client will listen for incoming connections
 * @param remotePort The port on the device where the desired service is listening
 * @param proto The protocol that will be used between final client and final server:
 *              this parameter accept Transport layer protocols (UDP/TCP) or Application layer (RTSP)
 * @return A boolean value, true if port mapping is correctly initialized, false otherwise
 * @see IotcAgent
 * @see TunnelProtocols
 */
bool portMap(IotcAgent *iotcAgent, unsigned short localPort, unsigned short remotePort,
                TunnelProtocols proto);

/**
 * @brief Discover devices in the same LAN of the client
 *
 * @param ctx The IotcCtx created using iotcInitClient()
 * @param onDiscoveryResults A callback invoked when discovery ends. Params are:
 *	- list The list of discovered devices
 */
bool lanDiscovery(IotcCtx *ctx, void (*onDiscoveryResults)(struct deviceDiscoveredList *));

/**
 * @brief Get a non trivial (loopback) IP for this device
 */
char *getMyLocalIP();

#endif /* __LIBRARY_H__ */
