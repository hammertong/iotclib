#/bin/sh
export BASE_PATH=/usr/local/urmetiotc_x86_64
export REMOTE_PORT=443
export IFACE_VPC=ens4				# VPC google cloud

get_vpc_iface_ip()
{
	/sbin/ip -4 addr | grep $IFACE_VPC | grep -oP '(?<=inet\s)\d+(\.\d+){3}'
}

get_first_available_port()
{
	lower_port=`cat /proc/sys/net/ipv4/ip_local_port_range | awk '{ print $1 }'`
	#lower_port=25
	upper_port=`cat /proc/sys/net/ipv4/ip_local_port_range | awk '{ print $2 }'`
	LPORT=""
	for p in `seq $lower_port $upper_port`
	do
		/bin/nc -z 0.0.0.0 $p
		if [ "$?" = "1" ]
		then
			LPORT=$p
			break
		fi
	done
	echo $LPORT
}

if [ "$1" = "" ]
then
	echo "Usage:"
	echo "$0 <UID>"
	exit 1 
fi

export UUID=`echo $1 | sed -e "s/\/dev\///g"`
export PID=`ps ax | grep client | grep TCP | grep $UUID | awk '{ print $1 }'`
export LPORT=`ps ax | grep client | grep TCP | grep $UUID | sed -e 's/.*client//g' | awk '{ print $2 }'`

if [ "$LPORT" = "" ]
then
	
	#
	# get the private network ip address
	#	
	#IP=`get_vpc_iface_ip`
	#if [ "$IP" = "" ]
 	#then
	#	IP="127.0.0.1"
	#fi

	#
	# get the first unused TCP port 
	#
	LPORT=`get_first_available_port`

	#
	# launch client
	#
	LD_LIBRARY_PATH=$BASE_PATH/lib $BASE_PATH/bin/client $UUID $LPORT $REMOTE_PORT TCP >/dev/null 2>&1 &
	#echo LD_LIBRARY_PATH=$BASE_PATH/lib $BASE_PATH/bin/client "$UUID $LPORT $REMOTE_PORT TCP" 

else

	if [ "$2" = "kill" ]
	then
		echo "killing $UUID client ..."
		kill -TERM $PID
	fi

fi

echo "$LPORT"

