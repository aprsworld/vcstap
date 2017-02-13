#inline
char xor_crc(char oldcrc, char data) {
	return oldcrc ^ data;
}

char EEPROMDataRead( int16 address, int8 *data, int16 count ) {
	char crc=0;

	while ( count-- != 0 ) {
		*data = read_eeprom( address++ );
		crc = xor_crc(crc,*data);
		data++;
	}
	return crc;
}

char EEPROMDataWrite( int16 address, int8 *data, int16 count ) {
	char crc=0;

	while ( count-- != 0 ) {
		/* restart_wdt() */
		crc = xor_crc(crc,*data);
		write_eeprom( address++, *data++ );
	}

	return crc;
}

void write_param_file() {
	int8 crc;


	/* write the config structure */
	crc = EEPROMDataWrite(PARAM_ADDRESS,(void *)&config,sizeof(config));
	/* write the CRC was calculated on the structure */
	write_eeprom(PARAM_CRC_ADDRESS,crc);

}

void write_default_param_file() {
	/* red LED for 1.5 seconds */
	timer.led_on_red=150;
//	fprintf(world,"# writing default parameters\r\n");

	config.revision='a';

	config.serial_prefix='A'; //SERIAL_PREFIX_DEFAULT;
	config.serial_number=4780; //SERIAL_NUMBER_DEFAULT;


	config.sensor_source=SENSOR_SOURCE_ONBOARD;

	config.world_to_xbee=0;
	config.world_to_xport=1;

	config.pair_serial_prefix=0;
	config.pair_serial_number=0;

	config.modbus_address=29;

	/* write them so next time we use from EEPROM */
	write_param_file();

}


void read_param_file() {
	int8 crc;

	crc = EEPROMDataRead(PARAM_ADDRESS, (void *)&config, sizeof(config)); 

//	fprintf(modem,"# read_param_file()\r\n");
//	print_param_file();

		
	if ( crc != read_eeprom(PARAM_CRC_ADDRESS) || config.revision<'a' || config.revision>'z' ) {
		write_default_param_file();
	}

}

