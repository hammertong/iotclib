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
echo "  2) sudo ./install_supervisor.sh"
echo ""


#ssh mntfrcud@iotc-nod1 -- tar -xzf $SF && ./install_supervisor.sh
#ssh mntfrcud@iotc-nod2 -- tar -xzf $SF && ./install_supervisor.sh
#ssh mntfrcud@iotc-nod3 -- tar -xzf $SF && ./install_supervisor.sh
#ssh mntfrcud@iotc-nod4 -- tar -xzf $SF && ./install_supervisor.sh

