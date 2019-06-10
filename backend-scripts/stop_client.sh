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
kill -TERM `ps ax | grep client | grep $1 | awk '{ print $1 }'`
rm -f /tmp/client_$2
exit 0

