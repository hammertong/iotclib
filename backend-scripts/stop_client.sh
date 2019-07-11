#!/bin/sh
if [ "$1" = "" ]
then
  echo "error"
  exit 1
fi
if [ "$2" = "" ]
then
  echo "error"
  exit 1
fi
[ -f "/tmp/client_$1" ] && rm -f "/tmp/client_$1"
[ -f "/tmp/client_$2" ] && rm -f "/tmp/client_$2"
CCPID=`ps ax | grep client | grep $1 | grep -v "grep" | grep -v "stop_client" | awk '{ print $1 }'`
if [ "$CCPID" = "" ]
then
  echo "-1"
  exit
fi
kill -TERM $CCPID
echo $CCPID 

