/*
 * Copyright (C) 2015 CSP Innovazione nelle ICT s.c.a r.l. (http://www.csp.it/)
 * All rights reserved.
 * 
 * [any text provided by the client]
 *
 */

/**
 * @file mqtt.h
 * @author Matteo Di Leo <matteo.dileo@csp.it>
 * @date 18/02/2015
 * @brief Urmet IoT mqtt signalling implementation
 *
 * The Urmet IoT mqtt signalling implementation
 * Here are placed all the functions used to subscribe to `device topic`
 * and to publish from device to signal events to server.
 * Mqtt implementation is based on Mosquitto library and uses ssl/tls.
 */

#ifndef __MQTT_H__
#define __MQTT_H__

//#ifndef IOTC_CLIENT

#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
#include <errno.h>
#endif

#include <mosquitto.h>

#include <openssl/ssl.h>

/**
 * @brief The context used for mqtt connection and messages exchange.
 *
 * This context is created by mqttNew(). When it is needed no more it should be freed
 * invoking mqttFree().
 * @see mqttNew()
 * @see mqttFree()
 */
typedef struct mqttCtx MqttCtx;

/**
 * @brief Create and initialize mqtt
 *
 * Create MqttCtx and starts network thread for manage mqtt communications.
 * Use CA, and my certificate and key files to initialize TLS. TLSv1 is used.
 * Mosquitto structure should be freed using mqttFree().
 * @see mqttFree()
 * @param deviceId The unique id of the device, used as `user agent`. This must be unique;
 *	same id used by two or more different devices confuses broker which disconnect both.
 * @param host The hostname of the broker
 * @param port The port of the broker to connect to (usually 1883)
 * @param CAFile The full path of CA certificate used to verify broker identity.
 * @param CAPath The path of directory where is contained CA certificate. Only one between
 *		CAFile and CAPath is strictly needed.
 * @param crtFile The full path of my certificate used by broker to verify my identity
 * @param keyFile The full path of my private key used to crypt my messages
 * @param connectCb The callback that will be called when connection is complete. Parameters are:
 *	- mqttCtx the MqttCtx instance making the callback
 *	- userData the user data provided as parameter in this funtion
 *	- result of operation (0 OK)
 * @param subscribeCb The callback that will be called when subscribe is complete. Parameters are:
 *	- mqttCtx the MqttCtx instance making the callback
 *	- userData the user data provided as parameter in this funtion
 *	- mid the message id of the subscribe message.
 *	- qosCount the number of granted subscriptions
 *	- granted an array of integers indicating the granted QoS for each of the subscriptions. Parameters are:
 * @param messageCb The callback that will be called when a new message is received
 *	- mqttCtx the MqttCtx instance making the callback
 *	- userData the user data provided as parameter in this funtion
 *	- message the message data received
 * @param disconnectCb The callback that will be called when connection stop
 *	- mqttCtx the MqttCtx instance making the callback
 *	- userData the user data provided as parameter in this funtion
 *	- rc reason of disconnection (0 disconnect invoked by client)
 * @param userData A pointer to data passed back to callbacks
 * @return A pointer to MqttCtx if success, NULL otherwise
 */
MqttCtx *mqttNew(char *deviceId, char *host, int port,
		char *CAFile, char *CAPath, char *crtFile, char *keyFile,
		void (*connectCb)(MqttCtx *, void *, int),
		void (*subscribeCb)(MqttCtx *, void *, int, int, const int *),
		void (*messageCb)(MqttCtx *, void *, const struct mosquitto_message *),
		void (*disconnectCb)(MqttCtx *, void *, int),
		void *userData);

/**
 * @brief Subscribe to mqtt topic
 *
 * Send a subscribe to broker for the specified topic
 * @see mqttNew()
 * @param mqttCtx A pointer to MqttCtx, correctly initialized using mqttNew()
 * @param topic The topic to subscribe to
 * @return true if success, false otherwise
 */
bool mqttSubscribe(MqttCtx *mqttCtx, char *topic);

/**
 * @brief Publish to mqtt topic
 *
 * Send a message for publishing on the specified topic
 * @see mqttNew()
 * @param mqttCtx A pointer to MqttCtx, correctly initialized using mqttNew()
 * @param topic The topic where to publish on
 * @param msg The message to publish
 * @return true if success, false otherwise
 */
bool mqttPublish(MqttCtx *mqttCtx, char *topic, char *msg);

/**
 * @brief Stop and free mqtt structures
 *
 * Stop thread for management of network messages from mqtt and free all structures used
 * @param mqttCtx A pointer to MqttCtx, correctly initialized using mqttNew()
 * @return true if success, false otherwise
 */
bool mqttFree(MqttCtx *mqttCtx);

//#endif // IOTC_CLIENT

#endif
