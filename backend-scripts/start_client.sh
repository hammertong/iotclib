#!/bin/sh
export BASE_PATH=/usr/local/urmetiotc_x86_64
RPORT=443
DEVICE=$1
LPORT=$2
if [ "$DEVICE" = "" ]
then
  echo "error"
  exit 1
fi
if [ "$LPORT" = "" ]
then
  echo "error"
  exit 1
fi
LD_LIBRARY_PATH=$BASE_PATH/lib $BASE_PATH/bin/client $DEVICE $LPORT $RPORT TCP >>/tmp/client_$LPORT 2>&1 &
#LD_LIBRARY_PATH=$BASE_PATH/lib $BASE_PATH/bin/client $DEVICE $LPORT $RPORT TCP 
echo $LOPORT
exit 0

