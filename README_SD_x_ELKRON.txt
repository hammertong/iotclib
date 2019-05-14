Bozza PortMapping

SW Running

	device su sentrale elkron 
		LWT_NOTIFIER ? 
		che eventi risultano sul server ? connessione mqtt e ...? 

	client DEVICE RPORT LPORT
		lport deve essere decisa

		FASE 1 : php proxy 127.0.0.1:LPORT
		FASE 2 : php proxy NODO_REGISTRATO:LPORT


REGISTRAZIONE DEVICE

device  +---------------- 80 / 443 -----------------+ 
        |                                           |
        |         www.cloud.urmet.com (L.B.)  ----------> + HTTPS 4043  nod4 +
1)      |                  POST --->                      |  CA signrequest  |
        |           <--- 200 CERT or 500      <---------- +------------------+         
        |                                           |
        +------------------ 4343 -------------------+
        |                                           |
        |        www.cloud.urmet.com (L.B.)         |
2)      |                GET Server List --->       |
        |           <---                            |
        |                                           |
        +-------------------------------------------+
        |                                           |
        |                                           |
        |                                           |


SCENARIO TUNNELING ELKRON
                                               
Elkron 1 ICE Tunnnel +------+------------------------+ TCP::127.0.0.1:port 1 +
Elkron 2 ICE Tunnnel        |                        | TCP::127.0.0.1:port 2 |
...                         |       MQTT::1889       | ...                   |
                            |---+ STUN/ICE::3478 +---|                       |     PHP passthru 
                            |                        |                       |         HTTPS 
                            |                        |                       |  www.cloud.urmet.com
                            |                        |                       | 
                            |                        |                       |
Elkron n ICE Tunnnel +------+------------------------+ TCP::127.0.0.1:port n +


