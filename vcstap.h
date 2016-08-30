#include <18F2680.h>
#device ADC=10
#device *=16

/* leave last nine pages alone for boot loader. first two words do the jump to the boot loader */
#build(reset=0xfdbc:0xfdbf)
#org 0xfdc0,0xffff {}


/*
		#define		BLPLP		9			;bootloader placement, pages from end
		#define		BLSIZEP		9			;bootloader size [pages], used by bootloader protection	
*/


/* EEPROM locations */
#define PARAM_CRC_ADDRESS 0x00
#define PARAM_ADDRESS     0x01


#include <stdlib.h>
//#FUSES INTRC_IO,NOPROTECT,WDT8192, NOIESO, NODEBUG
#use delay(clock=8000000)

/*
 Sensors  WorldData R33 R34 R35 R36 XBee | UART1    | UART2 | SCI_UART                  | STATUS
 on-board Ethernet  DNS DNS DNS DNS DNS  | inverter | world | (onboard XRW2G)           | done
 802.15.4 Ethernet  0   DNS DNS DNS YES  | inverter | world | (external XRW2G via xBee) | (testing)
 802.15.4 802.15.4  0   0   0   DNS YES  | inverter | world | (external XRW2G via xBee) | (testing)
 on-board 802.15.4  DNS 0   0   DNS YES  | inverter | world | (onboard XRW2G)           | done
 */



#use rs232(UART1,stream=rs232,baud=9600,xmit=PIN_C6,rcv=PIN_C7, errors)	 /* also connected to XPort transmit ... xport receive not connected */
#use i2c(master, sda=PIN_C4, scl=PIN_C3, FAST)




#use standard_io(A)
#use standard_io(B)
#use standard_io(C)

#define LED_GREEN                  PIN_B4
// #define LED_RED                    PIN_B5 repurposed as relay
#define RELAY_RED                  PIN_B5

#define XBEE_SLEEP                 PIN_A0
#define XBEE_NRTS                  PIN_A1
#define XBEE_NCTS                  PIN_A2

#define SYNC_OUT                   PIN_C5 /* really labeled sync in */

#define UART_IRQ                   PIN_B0
#define UART_RESET                 PIN_B1

#define TP14                       PIN_B3
#define TP15                       PIN_B2


#define TP_RED     PIN_B6
#define TP_ORANGE  PIN_B7


//#byte ANCON0=GETENV("SFR:ancon0")
//#byte ANCON1=GETENV("SFR:ancon1")



#define SERIAL_PREFIX_DEFAULT      'Z'
#define SERIAL_NUMBER_DEFAULT      6543

#define SENSOR_SOURCE_ONBOARD  0
#define SENSOR_SOURCE_WIRELESS 1


/* can baud rate registers */
//#byte BRGCON1=GETENV("SFR:brgcon1")
//#byte BRGCON2=GETENV("SFR:brgcon2")
//#byte BRGCON3=GETENV("SFR:brgcon3")