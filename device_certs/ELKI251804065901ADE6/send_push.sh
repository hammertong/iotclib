#curl -k -v -H "Content-Type: application/json" \
curl -k -v  -H "Content-Type: application/json" \
	--data-binary @./push.json \
	--cacert ./cacert.pem \
	--key ./device.key \
	--cert ./device.crt \
	-X POST https://www.cloud.elkron.com:4343/pn_raise_event/

