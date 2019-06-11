#!/bin/sh
#SF=setup_supervisor_`date +%Y%m%d`.tar.gz
#tar -czf $SF client *.sh
export BASE_PATH=/usr/local/urmetiotc_x86_64

for node in iotc-nod1 iotc-nod2 iotc-nod3 iotc-nod4
do
  echo "deployng to $node ..."  
  scp client backend-scripts/* mntfrcud@$node:deploy-bin
  ssh mntfrcud@$node -- sudo mv deploy-bin/* $BASE_PATH/bin/
done
exit 0 

