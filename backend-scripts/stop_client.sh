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
rm -f /tmp/client_$2
kill -TERM `ps ax | grep client | grep $1 | awk '{ print $1 }'`
exit 0

