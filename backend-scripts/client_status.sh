#!/bin/sh
while [ "1" = "1" ]
do
	read x
	#echo $x > /tmp/client_fifo
	/usr/local/urmetiotc_x86_64/bin/client_create.sh $(echo $x | sed 's/\(.*\)/\U\1/')
	#cat /tmp/client_fifo_return
done


