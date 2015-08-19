#include "vcstap.h"



typedef struct {
	int8 revision;

	int8 modbus_address;

	int8  serial_prefix;
	int16 serial_number;

	int8  pair_serial_prefix;
	int16 pair_serial_number;

	int8  sensor_source;
	int8  world_to_xbee;
	int8  world_to_xport;
} struct_config;

#define VCS_N_REGISTERS 40

typedef struct {
	int8  modbus_enable;
	int8  modbus_last_error;
	int8  factory_unlocked;
	int16 telem_age;

	int8  xrw2g_packet[128];
	int8  xrw2g_buff_pos;
	int16 xrw2g_age;

	/* VCS data as updated by the CAN bus */
	int8 vcs_register     [VCS_N_REGISTERS*4]; /* 40 32-bit registers */
	int16 vcs_query_age;
	int8 vcs_read_lock; /* 1 means nobody can modify the data */

	int16 led_on_red;
	int16 led_on_green;

	int8 dump_register;

	int8 now_telem;
} struct_timer;

/* global structures */
struct_config config;
struct_timer  timer;

#include "can.c"
#include "uart_sci16is740.c"
#include "live.c"
#include "modbus_slave_vcstap.c"
#include "interrupt.c"
#include "param.c"
#include "modbus_handler_vcstap.c"





void init() {
	setup_oscillator(OSC_8MHZ | OSC_INTRC);
	setup_adc(ADC_OFF);
	/* 
	setup_adc_ports(NO_ANALOGS); doesn't seem to work. 
	Manually set ANCON0 to 0xff and ANCON1 to 0x1f for all digital
	*/
//	ANCON0=0xff;
//	ANCON1=0x1f;
	
	
//	setup_comparator(NC_NC_NC_NC);

	setup_timer_2(T2_DIV_BY_16,61,2); // set 1 millisecond period with 8 MHz oscillator

	port_b_pullups(TRUE);
	delay_ms(10);
	
	output_low(SYNC_OUT);


	timer.modbus_enable=FALSE;


	/* xrw2g uart */
	uart_init();

	/* CAN interface to VCS */
	can_init();
	/* set receive filter so we only get data from CAN ID 3*/
//	can_set_mode(CAN_OP_CONFIG); 
//	can_set_id(RX0MASK,0x003,false); ???
//	can_set_mode(CAN_OP_NORMAL); 
   // mask bit n | filter bit n | message ID bit n | result
   //     0             x               x             accept
   //     1             0               0             accept
   //     1             0               1             reject
   //     1             1               0             reject
   //     1             1               1             accept
	/* receive and receiver error interrupts */
	enable_interrupts(INT_CANRX0); 
	enable_interrupts(INT_CANRX1); 
	enable_interrupts(INT_CANIRX);



	/* global structures */
	timer.factory_unlocked=0;
	timer.telem_age=0;

	timer.xrw2g_age=65535;
	timer.xrw2g_buff_pos=0;
	memset(timer.xrw2g_packet, 0, sizeof(timer.xrw2g_packet));

	timer.vcs_read_lock=0;
	timer.vcs_query_age=0;
	memset(timer.vcs_register, 0, sizeof(timer.vcs_register));

	timer.dump_register=255; 
	timer.now_telem=0;

	/* receive data from serial ports */
	enable_interrupts(INT_RDA);  /* inverter */
//	enable_interrupts(INT_RDA2); /* world (ethernet or xbee */

	/* timer0 is used for modbus handler */

	/* 1 millisecond timer */
	enable_interrupts(INT_TIMER2);


}

#define interrupt_enabled(x)  !!(*(make8(x,1) | 0xF00) & make8(x,0)) 

void read_data_xrw2g(void) {
	int8 c;

	if ( uart_kbhit() ) {
		while ( uart_kbhit() ) {
//			output_high(TP_ORANGE);
			c=uart_getc();
//			output_low(TP_ORANGE);
			timer.xrw2g_age=0;

			if ( timer.xrw2g_buff_pos < sizeof(timer.xrw2g_packet) ) {
				timer.xrw2g_packet[timer.xrw2g_buff_pos++]=c;
			}
		}
	}
}


void send_can_query(int8 queryRegister) {
	unsigned int32 can_id;
	unsigned int8 data[4];

	/* message ID 3 is query */
	can_id=(int32) 3;

	data[0]=queryRegister;
	data[1]=0x00;
	data[2]=0x00;
	data[3]=0x00; 

	if ( 0 == can_putd(can_id,data,4,0,FALSE,FALSE) ) {
//		timer.led_on_red=50;
	}

}


/* this is started after the bootloader is done loading or times out */
void main(void) {
	int8 last;

	/* normal device startup */
	init();
	read_param_file();
	
	write_default_param_file();

	/* modbus_init turns on global interrupts */
//	modbus_init();

	enable_interrupts(GLOBAL);

	fprintf(rs232,"# (world) vcstap.c %s - my serial %c%lu\r\n",__DATE__,config.serial_prefix,config.serial_number);
//	fprintf(rs232,"# (rs232) vcstap.c %s - my serial %c%lu\r\n",__DATE__,config.serial_prefix,config.serial_number);

	/* fast red, yellow, green */
	timer.led_on_red  =200;
	delay_ms(160);
	timer.led_on_green=400;
	delay_ms(160);
	timer.led_on_red=0;
	delay_ms(160);
	timer.led_on_green=0;


	last=0;

	/* main loop */
	for ( ; ; ) {
		restart_wdt();

		/* read data from our different sources */
		read_data_xrw2g();
		
		/* transmit buffer empty */
		if ( can_tbe() && timer.vcs_query_age>=25 ) {
			/* loop through our list of registers */
			if ( last == VCS_N_REGISTERS )
				last=0;

			send_can_query(last++);
//			send_can_query(10);
			timer.vcs_query_age=0;
		}


#if 0
		if ( timer.dump_register != 255 ) {
			/* takes about 4 milliseconds */
			disable_interrupts(INT_CANRX0);
			disable_interrupts(INT_CANRX1);

			fprintf(rs232,"# [%u] {%02x %02x %02x %02x}\r\n",
				timer.dump_register,
				timer.vcs_register[timer.dump_register<<2 + 0],
				timer.vcs_register[timer.dump_register<<2 + 1],
				timer.vcs_register[timer.dump_register<<2 + 2],
				timer.vcs_register[timer.dump_register<<2 + 3]
			);
//			fputc('*',rs232);

			timer.dump_register=255;

			enable_interrupts(INT_CANRX0);
			enable_interrupts(INT_CANRX1);
		}
#endif



//		modbus_process();

	    /*
	     * every 10 seconds we send a sync pulse which should trigger another packet from XRW2G
	     * if we have wirless sensors, then this doesn't matter
	     */
	    if ( timer.now_telem ) {
			timer.now_telem=0;
//			output_high(SYNC_OUT);
			live_send_vcs();
//			output_high(TP_ORANGE);
	    } else {
			output_low(SYNC_OUT);
//			output_low(TP_ORANGE);
	    }


		/* we have XRW2G data, and we haven't gotten any more in last 50 miliseconds */
		if ( 0 != timer.xrw2g_buff_pos && timer.xrw2g_age > 50 ) {
#if 1
			live_send_xrw2g();
#else
			fprintf(rs232,"# XRW2G %c%lu ",timer.xrw2g_packet[1],make16(timer.xrw2g_packet[2],timer.xrw2g_packet[3]));
			if ( 0 == live_send_xrw2g() ) {
				fprintf(rs232,"error-%c%lu ",timer.xrw2g_packet[1],make16(timer.xrw2g_packet[2],timer.xrw2g_packet[3]));
			} else {
				fprintf(rs232,"success-%c%lu ",timer.xrw2g_packet[1],make16(timer.xrw2g_packet[2],timer.xrw2g_packet[3]));
			}
			timer.telem_age=0;
#endif
		}

#if 0
		 /* we haven't sent live data for 32 seconds */
		if (  timer.telem_age > 32000 ) {
			live_send(1);
			timer.telem_age=0;
		}

	

		if ( ! timer.modbus_enable ) {
			/* 	
				Status LEDs: 
					Solid red if no VCS data
					Flash of red on bad packets from inverter (in packet parse routine)
					Solid green if XRW2G in last 12 seconds
			*/
//			if ( timer.inverter_conditions_age > 1025 ) {
//				timer.led_on_red=2000;
//			} 
	
			if (timer.xrw2g_age < 12000 ) {
				timer.led_on_green=2000;
			}
		}

		/* full time RED LED if default serial number */
//		if ( config.serial_prefix == SERIAL_PREFIX_DEFAULT && config.serial_number == SERIAL_NUMBER_DEFAULT ) {
//			timer.led_on_red=200;
//		}
#endif
	}
}

