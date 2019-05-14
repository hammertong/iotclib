#!/bin/sh
#
# Usage: 
# $0 <ELKI251804065901ADE6>
#  

# BEGIN USER CONFIGURABLE SECTION

export BASE_PATH=/usr/local/urmetiotc_x86_64
export URI_PATH=/tool/webapi/public/supervisor.php
export REMOTE_PORT=443				# TCP listener of exposed remote device  
#export IFACE_VPC=enp0s31f6 			# local ethernet test
#export IFACE_VPC=wlp2s0				# local wifi test
export IFACE_VPC=ens4				# VPC google cloud

export DBU=iotcadmin
export DBP=urM3t_Cl0ud_2014
export DBH=127.0.0.1
export DBN=ElkronCloud

export LOGFILE=/tmp/client_status_log_`date +%Y%m%d`.log

# END USER CONFIGURABLE SECTION

if [ "$1" = "" ]
then
	echo "Usage:"
	echo "$0 <UID>"
	echo "e.g.: $0 ELKI251804065901ADE6"
	exit 1
fi

UUID=$1

echoff()
{
	echo -n `date +%Y%m%d' '%H:%M:%S` >> $LOGFILE
	echo " $1" >> $LOGFILE
}


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
	return $LPORT
}

get_existing_portforwarding()
{
	mysql -N -s --user="$DBU" --password="$DBP" \
		--database="$DBN" --host="$DBH" \
		--execute="SELECT clientInternalIp, clientPort FROM Ice_PortMapping WHERE uid='$UUID';" \
		2>/dev/null
}

add_portforwarding()
{
	echoff "adding new $UUID port forwarder at $IP:$LPORT ..."
	mysql -N -s --user="$DBU" --password="$DBP" \
		--database="$DBN" --host="$DBH" \
		--execute="INSERT INTO Ice_PortMapping (uid, clientInternalIp, clientPort) VALUES ('$UUID', '$IP', $LPORT );" \
		2>/dev/null
}

IP=""
LPORT=""
RES=`get_existing_portforwarding $UUID`
IP=`echo $RES | awk '{ print $1 }'`
LPORT=`echo $RES | awk '{ print $2 }'`


if [ "$IP" = "" ]
then

	echoff "create new client agent locally ..."

	#
	# get the private network ip address
	#	
	IP=`get_vpc_iface_ip`

	#
	# get the first unused TCP port 
	#
	get_first_available_port
	LPORT=$?
	
	#
	# create new record on database
	#
	add_portforwarding

fi

wget --no-check-certificate "https://$IP$URI_PATH?UID=$UUID&LPORT=$LPORT" -O /tmp/client_status_wget >/dev/null 2>&1

#if [ "$?" = "0" ]
#then
#	echo "https://$IP:$LPORT"
#	exit 0
#else	
#	exit 1
#fi

echo "https://$IP:$LPORT"
