#!/bin/sh
for h in iotc-nod1 iotc-nod2 iotc-nod3 iotc-nod4
do
	ssh mntfrcud@$h -- ps ax | grep client | grep TCP | grep -v grep | awk '{ print $1" "$4" "$6" "$7  }' | sed -e "s/^/$h /g"
done
