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
 * @file testDevice.c
 * @author Matteo Di Leo
 * @date 09/09/2015
 * @brief A simple file to test device functionalities
 *
 * This simple file will be probably removed from production version.
 * It is used to test functionalities.
 */

//#ifndef IOTC_CLIENT

#include <stdio.h>
#include "library.h"

#ifdef ARM
#define BASE_PATH "./"
#elif GRAIN
#define BASE_PATH "./"
#else
#define BASE_PATH "/usr/local/iotc-dependencies/certs/"
#endif

int testDevice(int argc, char *argv[]) {
	if(argc == 3 && strlen(argv[1]) <= 20) {
		int init = iotcInitDevice(argv[1], argv[2]);
		if(init == -1) {
			printf("Init ERROR!\n");
		} else {
			printf("Clean exit...\n");
		}
	} else {
		printf("Invalid parameters.\nUsage: %s UID BASE_PATH\n\tUID: an alphanumeric string of max 20 characters (example: 4YW673A8A98E47BG111A)\n\tBASE_PATH: the path where configuration files can be readed/written (example ./)\n", argv[0]);
	}

/*	while(true) {
		char *useless = (char *)malloc(sizeof(1024));
		printf("sleep...%d\n", useless);
		sleep(2);
		free(useless);
	}*/
//	int init = iotcInitDevice("FVP9B9YE53A1VMPKPVPJ", BASE_PATH);
//	int init = iotcInitDevice("EFY98NRY9VU1BMPKPVC1", BASE_PATH);
//	int init = iotcInitDevice("E3K9BTP69BAH8NPKKVCJ", BASE_PATH);
//	int init = iotcInitDevice("EVU9AHNP4BA1BGPKPVW1", BASE_PATH); // Sergio per l'ESP
//	int init = iotcInitDevice("CLYTA9NP4DUHSN6KKB5J", BASE_PATH); // Scarafia
//	int init = iotcInitDevice("RASPIOTC111111111111", BASE_PATH); // Raspberry per Pinto
//	int init = iotcInitDevice("4YW673A8A98E47BG111A", BASE_PATH); // NVR RaySharp
//	int init = iotcInitDevice("BU8DKHNE4MYD9HTWMUW1", BASE_PATH); // M18 test

	return 0;
}

//#endif // IOTC_CLIENT
