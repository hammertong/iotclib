#!/bin/sh
export REMLOG=/var/log/syslog
export MLOG=/tmp/rem_mqtt_log
export FILTER=ELK

if [ "$1" != "" ]
then
  ssh $1 -- grep -E "PINGRESP.*$FILTER.*" $REMLOG > $MLOG
else
  grep -E "PINGRESP.*$FILTER.*" $REMLOG > $MLOG
fi

now=`date +%s`
for f in $(awk '{ print $9 }' < $MLOG | grep -v "mosqsub\|iOS" | sort | uniq)
do
  t=`grep "$f" $MLOG | grep PINGRESP |  tail -1 | awk '{ print $1" "$2" "$3 }'` 
  last=`date -d "$t" +%s`
  delta=`php -r "echo $now-$last;"`
  echo "$f\t$delta"
done

rm -f $MLOG

