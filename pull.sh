#!/bin/sh
BASE_PATH=/usr/local/urmetiotc_x86_64
node=$1
if [ "$node" = "" ]
then
  echo "Usage:"
  echo "$0 <node-hostname>"
  exit 1
fi
scp mntfrcud@$node:$BASE_PATH/bin/\{client_status.sh,start_client.sh,stop_client.sh,client_create.sh,check_certificate.sh\} backend-scripts/
