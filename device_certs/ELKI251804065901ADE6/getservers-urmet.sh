curl -k -v \
	--cacert ./cacert.pem \
	--key ./device.key \
	--cert ./device.crt \
	https://www.cloud.urmet.com:4343/devicegetservers/

