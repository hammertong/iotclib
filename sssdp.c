/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 * 
 * [any text provided by the client]
 *
 * sssdp.c
 *      Urmet IoT SSSDP implementation
 *
 * Authors:
 *      Matteo Di Leo <matteo.dileo@csp.it>
 */

#include "library.h"
#include "sssdp.h"
#include <libgssdp/gssdp.h>
#include <gio/gio.h>

#define SSSDP_DISCOVERY_TIMEOUT 8

struct deviceInfo {
	GSocketConnection *connection;
	GSSDPResourceGroup *resourceGroup;
	guint resourceRootDevice;
	guint resourceDevice;
	char *ip;
	guint16 port;
	char *urnDeviceType;
	char *schema;
	char *deviceName;
	char *vendor;
	char *vendorURL;
	char *deviceModel;
	char *modelNumber;
	char *deviceURL;
	char *serialNumber;
	char *uuid;
};
typedef struct deviceInfo DeviceInfo;

struct discoveryCtx {
	GSSDPClient *client;
	GSSDPResourceBrowser *resourceBrowser;
	void (*discoverEndCb)(struct deviceDiscoveredList *, void *);
	struct deviceDiscoveredList *list;
	void *userData;
};
typedef struct discoveryCtx DiscoveryCtx;

IOTC_PRIVATE gboolean sssdpRecvCb(GObject *sourceObject, GAsyncResult *res, gpointer userData) {
	DeviceInfo *device = (DeviceInfo *)userData;
	int sock = g_socket_get_fd(g_socket_connection_get_socket(device->connection));
	char buf[1024];
	recv(sock, buf, 1024, 0);

	char *format = "HTTP/1.1 200 OK\nConnection: close\n\n\
<?xml version=\"1.0\"?>\
<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\
	<specVersion>\
		<major>1</major>\
		<minor>0</minor>\
	</specVersion>\
	<device>\
		<deviceType>urn:%s:device:%s:1</deviceType>\
		<friendlyName>%s</friendlyName>\
		<manufacturer>%s</manufacturer>\
		<manufacturerURL>%s</manufacturerURL>\
		<modelDescription>%s</modelDescription>\
		<modelName>%s</modelName>\
		<modelNumber>%s</modelNumber>\
		<modelURL>%s</modelURL>\
		<serialNumber>%s</serialNumber>\
		<UDN>uuid:%s</UDN>\
		<presentationURL>http://%s:80</presentationURL>\
	</device>\
</root>\n\n";
	int descriptionLength = strlen(format)+strlen(device->schema)+strlen(device->urnDeviceType)+
			strlen(device->deviceName)+strlen(device->vendor)+strlen(device->vendorURL)+
			strlen(device->deviceModel)+strlen(device->deviceModel)+
			strlen(device->modelNumber)+strlen(device->deviceURL)+
			strlen(device->serialNumber)+strlen(device->uuid)+strlen(device->ip);
	char *description = (char *)malloc(descriptionLength);
	snprintf(description, descriptionLength, format, device->schema, device->urnDeviceType,
			device->deviceName, device->vendor, device->vendorURL, device->deviceModel,
			device->deviceModel, device->modelNumber, device->deviceURL,
			device->serialNumber, device->uuid, device->ip);
	send(sock, description, strlen(description), 0);
	g_object_unref(device->connection);
	free(description);
	return false;
}

IOTC_PRIVATE gboolean sssdpListenCb(GSocketService *service, GSocketConnection *connection, GObject *sourceObject, gpointer userData) {
	DeviceInfo *device = (DeviceInfo *)userData;
	device->connection = connection;
	GIOChannel *channel = g_io_channel_unix_new(g_socket_get_fd(g_socket_connection_get_socket(connection)));
	g_object_ref(connection);
	g_io_add_watch(channel, G_IO_IN, (GIOFunc)sssdpRecvCb, device);
	g_io_channel_unref(channel);
	return true;
}

IOTC_PRIVATE void updateSSDPResources(DeviceInfo *device) {
	// location is an url with this pattern: http://IP:port (http:// 7 chars, :port is max 6 chars)
	char *ssdpLocation = (char *)malloc(14+strlen(device->ip));
	char *ssdpRootDevice = (char *)malloc(23+strlen(device->uuid));
	char *ssdpType = (char *)malloc(32+strlen(device->urnDeviceType));
	char *ssdpDevice = (char *)malloc(39+strlen(device->uuid)+strlen(device->urnDeviceType));
	snprintf(ssdpLocation, 14+strlen(device->ip), "http://%s:%d", device->ip, device->port);
	snprintf(ssdpRootDevice, 23+strlen(device->uuid), "uuid:%s::upnp:rootdevice", device->uuid);
	snprintf(ssdpType, 15+strlen(device->schema)+strlen(device->urnDeviceType), "urn:%s:device:%s:1",
		device->schema, device->urnDeviceType);
	snprintf(ssdpDevice, 22+strlen(device->uuid)+strlen(device->schema)+strlen(device->urnDeviceType),
		"uuid:%s::urn:%s:device:%s:1", device->uuid, device->schema, device->urnDeviceType);

	if(device->resourceRootDevice>0)
		gssdp_resource_group_remove_resource(device->resourceGroup, device->resourceRootDevice);
	if(device->resourceDevice>0)
		gssdp_resource_group_remove_resource(device->resourceGroup, device->resourceDevice);

	device->resourceRootDevice = gssdp_resource_group_add_resource_simple(device->resourceGroup,
			"upnp:rootdevice", ssdpRootDevice, ssdpLocation);
	device->resourceDevice = gssdp_resource_group_add_resource_simple(device->resourceGroup,
			ssdpType, ssdpDevice, ssdpLocation);

	free(ssdpLocation);
	free(ssdpRootDevice);
	free(ssdpType);
	free(ssdpDevice);
}

IOTC_PRIVATE gboolean updateSSDPResourcesTimeoutCb(gpointer userData) {
	DeviceInfo *device = (DeviceInfo *)userData;

	char *newIP = getMyLocalIP();
	char *oldIP = device->ip;

	// replace ip and update resources if changed
	if(oldIP == NULL || (newIP != NULL && strcmp(oldIP, newIP) != 0)) {
		device->ip = newIP;
		updateSSDPResources(device);
		if(oldIP != NULL)
			free(oldIP);
	} else {
		if(newIP != NULL)
			free(newIP);
	}
	g_timeout_add_seconds(60, &updateSSDPResourcesTimeoutCb, userData);
	return G_SOURCE_REMOVE;
}

bool startSSDPServer(GMainLoop *gloop, char *urnDeviceType, char * schema, char *deviceName, char *vendor,
		char *vendorURL, char *deviceModel, char *modelNumber, char *deviceURL, char *serialNumber,
		char *uuid) {
	DeviceInfo *device;
	GSSDPClient *client;
	GError *error = NULL;
	GSocketService *service;
	char *ip = getMyLocalIP();
	if(ip == NULL) {
#ifdef DEBUG
		printf("SSDP server cannot get a valid IP\n");
#endif
		return false;
	}

	device = (DeviceInfo *)malloc(sizeof(DeviceInfo));
	if(device == NULL) {
#ifdef DEBUG
		printf("Cannot allocate SSDP server info\n");
#endif
		return false;
	}
	device->resourceRootDevice = 0;
	device->resourceDevice = 0;
	device->ip = ip != NULL ? ip : "";
	device->urnDeviceType = urnDeviceType != NULL ? urnDeviceType : "";
	device->schema = schema != NULL ? schema : "";
	device->deviceName = deviceName != NULL ? deviceName : "";
	device->vendor = vendor != NULL ? vendor : "";
	device->vendorURL = vendorURL != NULL ? vendorURL : "";
	device->deviceModel = deviceModel != NULL ? deviceModel : "";
	device->modelNumber = modelNumber != NULL ? modelNumber : "";
	device->deviceURL = deviceURL != NULL ? deviceURL : "";
	device->serialNumber = serialNumber != NULL ? serialNumber : "";
	device->uuid = uuid != NULL ? uuid : "";

	client = gssdp_client_new(g_main_loop_get_context(gloop), &error);
	if(error) {
#ifdef DEBUG
		printf("Error creating the SSDP server: %s\n", error->message);
#endif
		g_clear_error(&error);
		return false;
	}

	service = g_socket_service_new();
	device->port = g_socket_listener_add_any_inet_port(G_SOCKET_LISTENER(service), NULL, &error);
	if(device->port <= 0) {
#ifdef DEBUG
		printf("SSDP cannot create socket for description listener\n");
#endif
		g_clear_error(&error);
		return false;
#ifdef DEBUG
	} else {
		printf("Opened port for ssdp description: %d\n", device->port);
#endif
	}

	device->resourceGroup = gssdp_resource_group_new(client);
	if(device->resourceGroup == NULL) {
		g_object_unref(client);
		return false;
	}

	updateSSDPResources(device);
	gssdp_resource_group_set_available(device->resourceGroup, TRUE);

	g_signal_connect(service, "incoming", G_CALLBACK(sssdpListenCb), device);
	g_socket_service_start(service);

	g_timeout_add_seconds(60, &updateSSDPResourcesTimeoutCb, device);
	return true;
}

IOTC_PRIVATE gboolean stopDiscovery(gpointer userData) {
	DiscoveryCtx *ctx = (DiscoveryCtx *)userData;
	gssdp_resource_browser_set_active(ctx->resourceBrowser, false);
#ifdef DEBUG
	printf("Stop resource discovery\n");
#endif
	// invoke user cb passing found list
	if(ctx->discoverEndCb != NULL)
		ctx->discoverEndCb(ctx->list, ctx->userData);

	g_object_unref(ctx->resourceBrowser);
	g_object_unref(ctx->client);
	free(ctx);
	return G_SOURCE_REMOVE;
}

IOTC_PRIVATE void resourceAvailableCb(GSSDPResourceBrowser *resourceBrowser, char *usn,
		GList *locations, gpointer userData) {
	DiscoveryCtx *ctx = (DiscoveryCtx *)userData;
#ifdef DEBUG
	printf("Resource found\n  USN: %s\n", usn);
	GList *l;
	for(l = locations; l != NULL; l = l->next)
		printf("  Location: %s\n", (char *)l->data);
#endif
	if(locations != NULL) {
		char ip [51];
		char uuid[21];
		if(sscanf(locations->data, "%*[^:]://%50[^/]", ip) == 1) {
			char *lastColon = strrchr(ip, ':');
			if(lastColon != NULL)
				*lastColon = '\0';
			ip[50] = '\0';
		} else {
			return;
		}
		if(sscanf(usn, "uuid:%20[^:]", uuid) == 1) {
			uuid[20] = '\0';
		} else {
			return;
		}

		// add the resource to the head of list
		struct deviceDiscoveredList *newResource =
				(struct deviceDiscoveredList *)malloc(sizeof(struct deviceDiscoveredList));
		newResource->uuid = strdup(uuid);
		newResource->ip = strdup(ip);
		newResource->urnDeviceType = strdup(usn);
		newResource->next = ctx->list;
		ctx->list = newResource;
/*	} else {
		struct deviceDiscoveredList *newResource =
			(struct deviceDiscoveredList *)malloc(sizeof(struct deviceDiscoveredList));
		newResource->uuid = strdup("");
		newResource->ip = strdup("");
		newResource->urnDeviceType = strdup(usn);
		newResource->next = ctx->list;
		ctx->list = newResource;*/
	}
}

bool startSSDPDiscovery(GMainLoop *gloop, char *urnDeviceType,
		void (*discoverEndCb)(struct deviceDiscoveredList *, void *),
		void *userData) {
	GSSDPClient *client;
	GSSDPResourceBrowser *resourceBrowser;
	GError *error = NULL;

	// initialize ssdp client
	client = gssdp_client_new(g_main_loop_get_context(gloop), &error);
	if(error) {
#ifdef DEBUG
		printf("Error creating the SSDP client: %s\n", error->message);
#endif
		g_clear_error(&error);
		return false;
	}

	if(urnDeviceType == NULL)
		urnDeviceType = GSSDP_ALL_RESOURCES;
	resourceBrowser = gssdp_resource_browser_new(client, urnDeviceType);
	if(resourceBrowser == NULL) {
#ifdef DEBUG
		printf("Cannot allocate SSDP resource browser\n");
#endif
		g_object_unref(client);
		return false;
	}

	// allocate context passed as userData
	DiscoveryCtx *ctx = (DiscoveryCtx *)malloc(sizeof(DiscoveryCtx));
	if(ctx == NULL) {
#ifdef DEBUG
		printf("Cannot allocate SSDP discovery context\n");
#endif
		g_object_unref(resourceBrowser);
		g_object_unref(client);
		return false;
	}
	ctx->client = client;
	ctx->resourceBrowser = resourceBrowser;
	ctx->discoverEndCb = discoverEndCb;
	ctx->list = NULL;
	ctx->userData = userData;

	// bind signal resource-available and a timeout to stop discovery
	g_signal_connect(resourceBrowser, "resource-available", G_CALLBACK(resourceAvailableCb), ctx);
	gssdp_resource_browser_set_active(resourceBrowser, true);

	g_timeout_add_seconds(SSSDP_DISCOVERY_TIMEOUT, &stopDiscovery, ctx);

	return true;
}
