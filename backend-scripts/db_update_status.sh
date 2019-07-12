#!/bin/sh
if [ "$3" = "" ]
then
	echo "Usage:"
	echo "$0 <UID> <STATUS> <PORT>"
	exit 1
fi

export UUID=$1
export STATUS=$2
export PORT=$3
export IP_ADDR=`hostname -I | awk '{ print $1 }'`
export SQL="INSERT INTO IoTC_P2P( uid, status, internal_ip, port) "
export SQL="$SQL VALUES ( '$UUID', '$STATUS', '$IP_ADDR', $PORT ) "
export SQL="$SQL ON DUPLICATE KEY "
export SQL="$SQL UPDATE status = '$STATUS', internal_ip = '$IP_ADDR', port = $PORT"

#mysql -u iotcadmin -purM3t_Cl0ud_2014 UrmetCloud
echo "Execute SQL -> $SQL"
/usr/bin/mysql -h 127.0.0.1 -P 3306 -u iotcadmin -purM3t_Cl0ud_2014 -s ElkronCloud -e "$SQL"
