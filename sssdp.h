/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 * 
 * [any text provided by the client]
 *
 */

/**
 * @file sssdp.h
 * @author Matteo Di Leo <matteo.dileo@csp.it>
 * @date 12/03/2015
 * @brief Urmet IoT SSSDP implementation
 *
 * The Urmet IoT SSSDP (Super Simple Service Discovery) implementation
 * Here are placed all the functions used by ssdp server and client
 * for discovery of Urmet DigitalSecurityCameras and other devices
 * Implementation is based on gssdp-1.0
 */

#ifndef __SSSDP_H__
#define __SSSDP_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <nice/agent.h>

/**
 * @brief Start the SSDP server (used by device)
 *
 * Device can use this function to start and configure the SSDP server which announce
 * device presence and automatically respond to discovery queries in LAN
 * Service is permanent and not intended to be stoppable
 * @param gloop The Gnome Main Loop used by the service to invoke callbacks and for networking operations
 * @param urnDeviceType The device type (ex.: "DigitalSecurityCamera"), can be null or empty
 * @param schema The schema (ex.: "schemas-urmet-com"), can be null or empty
 * @param deviceName The device name (ex.: "Camera", "HNVR"), can be null or empty
 * @param vendor The vendor name (ex.: "Urmet"), can be null or empty
 * @param vendorURL The url where is possible to find vendor references (ex.: "http://www.cloud.urmet.com"),
 *	can be null or empty
 * @param deviceModel The device model (ex.: "M11"), can be null or empty
 * @param modelNumber The number of model (ex.: "1093/184"), can be null or empty
 * @param deviceURL The url where is possible to find device references, can be null or empty
 * @param serialNumber A serial number, can be null or empty
 * @param uuid An uuid, or similar, used to identify the device (ex.: the IoT ID)
 * @return A boolean value: true if the server has been correctly initialized, false otherwise
 */
bool startSSDPServer(GMainLoop *gloop, char *urnDeviceType, char *schema, char *deviceName, char *vendor,
		char *vendorURL, char *deviceModel, char *modelNumber, char *deviceURL, char *serialNumber,
		char *uuid);

/**
 * @brief Start SSDP discovery (used by client)
 *
 * Client can use this function to start asynchronous discovery operations.
 * On discovery end (usually in few seconds) discoveryEndCb is invoked,
 * passing as argument a NULL terminated list composed by struct deviceDiscoveredList,
 * the first element can also be null if no device has been discovered.
 * @param gloop The Gnome Main Loop used by the service to invoke callbacks and for networking operations
 * @param urnDeviceType The device type used to filter results or NULL
 * @param discoverEndCb The callback used for collecting results
 * @param userData Data passed back to callback
 */
bool startSSDPDiscovery(GMainLoop *gloop, char *urnDeviceType,
		void (*discoverEndCb)(struct deviceDiscoveredList *, void *),
		void *userData);

#endif
