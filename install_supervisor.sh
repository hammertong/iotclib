cp client /usr/local/urmetiotc_x86_64/bin/
chmod +x /usr/local/urmetiotc_x86_64/bin/client
rm -f /usr/local/urmetiotc_x86_64/bin/supervisor
ln -s /usr/local/urmetiotc_x86_64/bin/client /usr/local/urmetiotc_x86_64/bin/supervisor
cp supervisor_ctl.sh /usr/local/urmetiotc_x86_64/bin/ 
chmod +x /usr/local/urmetiotc_x86_64/bin/supervisor_ctl.sh
cp client_status.sh /usr/local/urmetiotc_x86_64/bin/
chmod +x /usr/local/urmetiotc_x86_64/bin/client_status.sh
rm -f /usr/bin/supervisor
rm -f /usr/bin/supervisor_ctl
rm -f /usr/bin/client_status
ln -s /usr/local/urmetiotc_x86_64/bin/supervisor /usr/bin/supervisor
ln -s /usr/local/urmetiotc_x86_64/bin/supervisor_ctl.sh /usr/bin/supervisor_ctl
ln -s /usr/local/urmetiotc_x86_64/bin/client_status.sh /usr/bin/client_status


