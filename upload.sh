#!/bin/sh
SF=setup_supervisor_`date +%Y%m%d`.tar.gz
#SF=setup_supervisor.tar.gz

tar -czf $SF client *.sh

scp $SF mntfrcud@iotc-nod3:
scp $SF mntfrcud@iotc-nod1:
scp $SF mntfrcud@iotc-nod2:
scp $SF mntfrcud@iotc-nod4:

echo ""
echo "Excecute on the 4 target machines:"
echo "  1) tar -xzf $SF"
echo "  2) sudo cp -f client /usr/local/urmetiotc_x86_64/bin/ && sudo cp -f *.sh /usr/local/urmetiotc_x86_64/bin/ && sudo chmod +x /usr/local/urmetiotc_x86_64/bin/client*" 

