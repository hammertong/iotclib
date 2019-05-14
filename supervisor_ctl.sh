#!/bin/sh
export LD_LIBRARY_PATH=/usr/local/urmetiotc_x86_64/lib
export SV=/usr/local/urmetiotc_x86_64/bin/supervisor

#export FOREGROUND=0
export FOREGROUND=1

start()
{
	echo "starting ..."
	if [ "$FOREGROUND" = "1" ]
	then
		$SV
	else
		#$SV > /tmp/supervisor_stdout 2>&1 &
		nohup $SV >/tmp/supervisor_stdout 2>&1 &
		#disown
	fi
}

stop()
{
	echo "stopping ..."
	killall supervisor
}

case "$1" in
    start)
    	start
    	;;  
    stop)
    	stop
    	;;
	*)
		echo "Usage: $0 {start|stop}"
    	exit 1
esac
