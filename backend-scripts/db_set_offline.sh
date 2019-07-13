#!/bin/sh
if [ "$1" = "" ]
then
	echo "Usage:"
	echo "$0 <UID>"
	exit 1
fi

export UUID=$1
export IP_ADDR=`hostname -I | awk '{ print $1 }'`
export SQL="UPDATE  IoTC_P2P set status = 'offline' where uid = '$UUID'"
#mysql -u iotcadmin -purM3t_Cl0ud_2014 UrmetCloud
echo "Execute SQL -> $SQL"
/usr/bin/mysql -h 127.0.0.1 -P 3306 -u iotcadmin -purM3t_Cl0ud_2014 -s ElkronCloud -e "$SQL"
