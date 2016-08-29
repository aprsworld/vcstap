#vcstap programming notes

Programming XRW2G Portion:
* meProg
* PIC18F4523
* world/firmwareProduction/nexucom/XRW2G.ctiw.20130416.hex


Programming ds30Loader into rsTap portion:
* meProg
* PIC18F2680
* world/firmwareTesting/vcsTap_ds30Loader/vcsTap_ds30loader.HEX


Programming vcsTap firmware:
* You will need firmware image with hardcoded serial number
* Connect RS-232 to DE9 connector via null modem cable
* ds30 Loader - Version: 1.5.1 (engine 2.2.2)
* Device: PIC18F  2680
* Baud: 57600


Programming Xport:
* Use XPort programming linux box
* ./program.nexucom 192.168.100.x
* telnet 192.168.100.x 9999
* 0 Server
* Change DHCP device name to: vcsTap


Note: Can make new Xport Settings files if programming several units so DHCP name doesn't have to be changed on each one.
