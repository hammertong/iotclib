#!/bin/sh
if [ "$1" = "debug" ]
then
  gLD_LIBRARY_PATH=/usr/local/urmetiotc_x86_64/lib gdb --args `pwd`/device ELKI251804065901ADE6 device_certs/ELKI251804065901ADE6/
elif [ "$1" = "memcheck" ]
then
  LOGFILE=valgrind.log
  echo "Valgrind memcheck logging to $logfile ..."
  G_SLICE=always-malloc G_DEBUG=gc-friendly LD_LIBRARY_PATH=/usr/local/urmetiotc_x86_64/lib  valgrind -v --tool=memcheck --leak-check=full --num-callers=40 --log-file=$LOGFILE `pwd`/device ELKI251804065901ADE6 device_certs/ELKI251804065901ADE6/
else
  LD_LIBRARY_PATH=/usr/local/urmetiotc_x86_64/lib \
	./device ELKI251804065901ADE6 device_certs/ELKI251804065901ADE6/
fi



