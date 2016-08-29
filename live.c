int16 crc_chk(int8 *data, int8 length) {
	int8 j;
	int16 reg_crc=0xFFFF;

	while ( length-- ) {
		reg_crc ^= *data++;

		for ( j=0 ; j<8 ; j++ ) {
			if ( reg_crc & 0x01 ) {
				reg_crc=(reg_crc>>1) ^ 0xA001;
			} else {
				reg_crc=reg_crc>>1;
			}
		}	
	}
	
	return reg_crc;
}

/* send CAN registers in RSTap data format */
void live_send_vcs(void) {
	static int16 sequenceNumber=0;
	int16 lCRC;
	int8 i,j;
	int8 buff[177];

	memset(buff,0,sizeof(buff));

	buff[0]='#';
	buff[1]=config.serial_prefix;
	buff[2]=make8(config.serial_number,1);
	buff[3]=make8(config.serial_number,0); 
	buff[4]=255; /* tell packet length to be read from 6 and 7 */
	buff[5]=18; /* packet type */
	buff[6]=0;
	buff[7]=179; /* 17 bytes header + 160 bytes CAN data + 2 bytes CRC */

	buff[8]=make8(sequenceNumber,1);
	buff[9]=make8(sequenceNumber,0);

	/* device info */
	/* WorldData device type identifier (16-bit) .... 1600 for VCSTap */
	buff[10]=make8(1600,1);
	buff[11]=make8(1600,0);
	/* manufacturers serial number */
	buff[12]=0;
	buff[13]=config.pair_serial_prefix;
	buff[14]=make8(config.pair_serial_number,1);
	buff[15]=make8(config.pair_serial_number,0);
	/* status of the data read  .... no error */
	buff[16]=0;

	/* shut down CAN reception and copy CAN registers over */
	disable_interrupts(INT_CANRX0);
	disable_interrupts(INT_CANRX1);
	/* do our endian swap one word at a time */
	for ( i=0 ; i<160 ; i+=4 ) {
		j=i + 17;

		buff[j+0]=timer.vcs_register[i+1];
		buff[j+1]=timer.vcs_register[i+0];
		buff[j+2]=timer.vcs_register[i+3];
		buff[j+3]=timer.vcs_register[i+2];
	}

	/* set fault indicator LED */

	/* any fault */
//	if ( timer.vcs_register[30*4] || timer.vcs_register[30*4+1] || timer.vcs_register[30*4+2] || timer.vcs_register[30*4+3] ) {
	/* system state 6 (FAULT) */
	if ( 6==timer.vcs_register[19*4+3] && 0==timer.vcs_register[19*4+2] && 0==timer.vcs_register[19*4+1] && 0==timer.vcs_register[19*4+0] ) {
		output_high(RELAY_RED);
	} else {
		output_low(RELAY_RED);
	}

	enable_interrupts(INT_CANRX0);
	enable_interrupts(INT_CANRX1);

	/* compute CRC on header and result data */
	lCRC=crc_chk(buff+1,sizeof(buff)-1);

	/* send buff, qbuff.rResult, CRC */
	for ( i=0 ; i<sizeof(buff) ; i++ ) {
		fputc(buff[i],rs232);
	}	
	fputc(make8(lCRC,1),rs232);
	fputc(make8(lCRC,0),rs232);

	sequenceNumber++;

	output_high(SYNC_OUT);
}

/*		
'#'                 0  STX
UNIT ID PREFIX      1  First character (A-Z) for serial number
UNIT ID MSB         2  high byte of sending station ID
UNIT ID LSB         3  low byte of sending station ID
PACKET LENGTH       4  number of byte for packet including STX through CRC
PACKET TYPE         5  type of packet we are sending, 27
SEQUENCE MSB        6
SEQUENCE LSB        7

CRC MSB              high byte of CRC on everything after STX and before CRC
CRC LSB              low byte of CRC
*/


int1 live_send_xrw2g() {
	int16 lCRC, rCRC;
	int8 i;
	int1 valid=1;

//	fprintf(rs232,"# (live) xrw2g_packet{=0x%02X, 0x%02X, 0x%02X}\r\n",timer.xrw2g_packet[1],timer.xrw2g_packet[2],timer.xrw2g_packet[3]);



	/* if wireless sensors, we might have heard something else and gotten in here. In that case, we
	 drop back out and try again*/
	if ( SENSOR_SOURCE_WIRELESS == config.sensor_source ) {
	    valid=0;

	    /* check packet type */
	    if ( 23==timer.xrw2g_packet[5] && timer.xrw2g_buff_pos>10 ) {
			/* is XRW2G packet */
			if ( 0==config.pair_serial_prefix && 0==config.pair_serial_number ) {
			    /* valid packet from an XRW2G, and we don't care which one */
			    valid=1;
			} else if ( timer.xrw2g_packet[1] == config.pair_serial_prefix && make16(timer.xrw2g_packet[2],timer.xrw2g_packet[3]) == config.pair_serial_number ) {
			    valid=1;
			}
	    }
	}

	if ( 0 == valid ) {
	    /* clear buffer and we'll try again next time */
	    timer.xrw2g_buff_pos=0;
	    return false;
	}

/*		
'#'                   0  STX
UNIT ID PREFIX        1  First character (A-Z) for serial number
UNIT ID MSB           2  high byte of sending station ID
UNIT ID LSB           3  low byte of sending station ID
PACKET LENGTH         4  number of byte for packet including STX through CRC
PACKET TYPE           5  type of packet we are sending, 23
SEQUENCE MSB          6
SEQUENCE LSB          7
(snip)
CRC MSB               96 high byte of CRC on everything after STX and before CRC
CRC LSB               97 low byte of CRC
	config.serial_prefix='Z';
	config.serial_number=9876;
*/
	/* check for valid CRC */
	if ( timer.xrw2g_buff_pos>=98 ) {
		rCRC = make16(timer.xrw2g_packet[96],timer.xrw2g_packet[97]);
		lCRC=crc_chk(timer.xrw2g_packet+1,95);

		if ( lCRC != rCRC ) {
		    /* clear buffer and we'll try again next time */
		    timer.xrw2g_buff_pos=0;
		    return false;
		}
	}

//	fprintf(rs232,"@ (sp=%c) (sn=%lu) (buff_pos=%u) @\r\n",timer.xrw2g_packet[1],make16(timer.xrw2g_packet[2],timer.xrw2g_packet[3]),timer.xrw2g_buff_pos);
	/* check for default serial number */
	if ( 'Z' == timer.xrw2g_packet[1] && 9876 == make16(timer.xrw2g_packet[2],timer.xrw2g_packet[3]) && timer.xrw2g_buff_pos>=98 ) {
		/* valid XRW2G packet with default serial number ... now we overwrite with our serial number */
		timer.xrw2g_packet[1]=config.serial_prefix;
		timer.xrw2g_packet[2]=make8(config.serial_number,1);
		timer.xrw2g_packet[3]=make8(config.serial_number,0);

		/* and re-calculate and replace CRC */
		lCRC=crc_chk(timer.xrw2g_packet+1,95);
		timer.xrw2g_packet[96]=make8(lCRC,1);		timer.xrw2g_packet[97]=make8(lCRC,0);
	}



	for ( i=0 ; i<98 ; i++ ) {
	    if ( config.world_to_xport )
			fputc(timer.xrw2g_packet[i],rs232);
	    if ( config.world_to_xbee )
			uart_putc(timer.xrw2g_packet[i]);
	}

	timer.xrw2g_buff_pos=0;
	return true;
}

