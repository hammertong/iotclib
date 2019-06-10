
COUNT THREADS
ps huH p <PID>



VLAGRIND MEMCHECK:

G_SLICE=always-malloc G_DEBUG=gc-friendly LD_LIBRARY_PATH=/usr/local/urmetiotc_x86_64/lib  valgrind -v --tool=memcheck --leak-check=full --num-callers=40 --log-file=valgrind.log `pwd`/device ELKI251804065901ADE6 device_certs/ELKI251804065901ADE6/

