#int_timer2
void isr_1ms(void) {
	static int16 telem_count=0;
	output_high(TP_RED);
	
	if ( timer.vcs_query_age < 65535 )
		timer.vcs_query_age++;

	if ( timer.vcs_last_data_age < 65535 )
		timer.vcs_last_data_age++;


	/* data ages */
	if ( timer.xrw2g_age < 65535 )
		timer.xrw2g_age++;


	if ( telem_count < 10000 ) {
		telem_count++;
	} else {
		timer.now_telem=1;
		telem_count=0;
	}


	/* LED Green */
	if ( 0==timer.led_on_green ) {
		output_low(LED_GREEN);
	} else {
		output_high(LED_GREEN);
		timer.led_on_green--;
	}

#if 0
	/* LED Red */
	if ( 0==timer.led_on_red ) {
		output_low(LED_RED);
	} else {
		output_high(LED_RED);
		timer.led_on_red--;
	}
#endif

	output_low(TP_RED);
}

#int_rda
void isr_serial_inverter(void) {
	int8 c;
	output_high(TP_ORANGE);

	c=fgetc(rs232);

	if ( timer.modbus_enable )  {
		if ( ! modbus_serial_new) {
			if ( modbus_serial_state == MODBUS_GETADDY) {
				modbus_serial_crc.d = 0xFFFF;
				modbus_rx.address = c;
				modbus_serial_state++;
				modbus_rx.len = 0;
				modbus_rx.error=0;
			} else if ( modbus_serial_state == MODBUS_GETFUNC) {
				modbus_rx.func = c;
				modbus_serial_state++;
			} else if(modbus_serial_state == MODBUS_GETDATA) {
				if (modbus_rx.len>=MODBUS_SERIAL_RX_BUFFER_SIZE) {
					modbus_rx.len=MODBUS_SERIAL_RX_BUFFER_SIZE-1;
				}
				modbus_rx.data[modbus_rx.len]=c;
				modbus_rx.len++;
			}
	
			modbus_calc_crc(c);
			modbus_enable_timeout(TRUE);
		}
	}
	
	output_low(TP_ORANGE);
}



#inline
void can_receive(void) {
	int8 buffer[8];
	int32 rx_id;
	int8 rx_len;
	struct rx_stat rx;


	if ( can_getd(rx_id,&buffer,rx_len,rx) ) {
		/* got something! */
		timer.led_on_green=50;

		/* VCS is ID ... ignore everything else */
		if ( 3 != rx_id )
			return;

		if ( 8 != rx_len )
			return;

		/* make sure register number is in range */
		if ( buffer[0] >= VCS_N_REGISTERS ) 
			return;

		timer.dump_register=buffer[0];

		/* re-use rx_len */
		rx_len = buffer[0]<<2;

		/* copy CAN data into appropriate register */
		timer.vcs_register[rx_len + 0]=buffer[4];
		timer.vcs_register[rx_len + 1]=buffer[5];
		timer.vcs_register[rx_len + 2]=buffer[6];
		timer.vcs_register[rx_len + 3]=buffer[7];

		/* reset last data received timer */
		timer.vcs_last_data_age=0;
	}
}


#INT_CANRX0
void isr_can_rx0(void) {
	output_high(TP_ORANGE);
	can_receive();
	output_low(TP_ORANGE);
}

#INT_CANRX1
void isr_can_rx1(void) {
	output_high(TP_ORANGE);
	can_receive();
	output_low(TP_ORANGE);
}


#INT_CANIRX
/* CAN packet error lights up RED led for two seconds */
void isr_canirx() {
	clear_interrupt(INT_CANIRX);
//	timer.led_on_red=2000;
}