#curl -k -v -H "Content-Type: application/json" \
curl -k -v  -H "Content-Type: application/json" \
	-H "Host: www.cloud.elkron.com:4343" \
	--data-binary @./push.json \
	--cacert ./cacert.pem \
	--key ./device.key \
	--cert ./device.crt \
	-X POST https://35.195.29.62:4343/pn_raise_event/

