#!/bin/sh

scp *.sh mntfrcud@iotc-nod1:
scp *.sh mntfrcud@iotc-nod2:
scp *.sh mntfrcud@iotc-nod4:
scp *.sh mntfrcud@iotc-nod3:

ssh mntfrcud@iotc-nod1 -- cp client_* /usr/local/urmetiotc_x86_64/bin/ && chmod +x /usr/local/urmetiotc_x86_64/bin/\*.sh 
ssh mntfrcud@iotc-nod2 -- cp client_* /usr/local/urmetiotc_x86_64/bin/ && chmod +x /usr/local/urmetiotc_x86_64/bin/\*.sh 
ssh mntfrcud@iotc-nod4 -- cp client_* /usr/local/urmetiotc_x86_64/bin/ && chmod +x /usr/local/urmetiotc_x86_64/bin/\*.sh 
ssh mntfrcud@iotc-nod3 -- cp client_* /usr/local/urmetiotc_x86_64/bin/ && chmod +x /usr/local/urmetiotc_x86_64/bin/\*.sh 

