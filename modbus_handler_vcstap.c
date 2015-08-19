#define MAX_DATA_REGISTER   100

#define MIN_CONFIG_REGISTER 1000
#define MAX_CONFIG_REGISTER 1011



int16 map_modbus(int16 addr) {
	int16 val;
	int8 i;

	/* VCS registers */
	if ( addr >= 3 && addr <= 83 ) {
		addr -= 3;

		i=addr % 2;
		if ( 1 == i )
			addr -= 1;

		addr = addr >> 1;

		timer.vcs_read_lock=1;
		

		return 0;
	}

	/* XRW2G data */

	switch ( addr ) {
		
		/* data */
		case   0: return 0; /* VCS control register ... always reads as 0 */
		case   1: return timer.vcs_query_age;
		case   2: return timer.xrw2g_age;


		/* configuration */
		case 1000: return config.serial_prefix;
		case 1001: return config.serial_number;
		case 1002: return 'V';
		case 1003: return 'C';
		case 1004: return 'S';
		case 1005: return  0;
		case 1006: return config.modbus_address;
		case 1007: return config.sensor_source;
		case 1008: return config.pair_serial_prefix;
		case 1009: return config.pair_serial_number;
		case 1010: return config.world_to_xbee;
		case 1011: return config.world_to_xport;

	   
		/* we should have range checked, and never gotten here */
		default: return 65535;
	}

}


int8 modbus_valid_read_registers(int16 start, int16 end) {
	if ( start <= MAX_DATA_REGISTER && end <= MAX_DATA_REGISTER+1 ) 
		return 1;

	if ( start >= MIN_CONFIG_REGISTER && end <= MAX_CONFIG_REGISTER+1 )
		return 1;

	return 0;
}

int8 modbus_valid_write_registers(int16 start, int16 end) {
	/* factory unlock */
	if ( 19999==start && 20000==end)
		return 1;

	/* write eeprom */
	if ( start >= 1998 && end <= 2000+1 )
		return 1;
	
	if ( start >= MIN_CONFIG_REGISTER && end <= MAX_CONFIG_REGISTER+1 )
		return 1;
	
	return 0;
}

void modbus_read_register_response(int8 address, int16 start_address, int16 register_count ) {
	int16 i;
	int16 l;

	modbus_serial_send_start(address, FUNC_READ_HOLDING_REGISTERS);
	modbus_serial_putc(register_count*2);


	for( i=0 ; i<register_count ; i++ ) {
		l=map_modbus(start_address+i);
		modbus_serial_putc(make8(l,1));
  		modbus_serial_putc(make8(l,0));
	}

	modbus_serial_send_stop();
}

/* 
try to write the specified register
if successful, return 0, otherwise return a modbus exception
*/
exception modbus_write_register(int16 address, int16 value) {

	/* if we have been unlocked, then we can modify serial number */
	if ( timer.factory_unlocked ) {
		if ( 1000 == address ) {
			config.serial_prefix=value;
			return 0;
		} else if ( 1001 == address ) {
			config.serial_number=value;
			return 0;
		}
	}

	/* publicly writeable addresses */
	switch ( address ) {
		case 1006:
			/* Modbus address {0 to 127} */
			if ( value > 127 ) return ILLEGAL_DATA_VALUE;
			config.modbus_address=value;
			break;
                case 1007:
                        if ( value > 1 ) return ILLEGAL_DATA_VALUE;
                        config.sensor_source=value;
                        break;
                case 1008:
                        if ( value > 255 ) return ILLEGAL_DATA_VALUE;
                        config.pair_serial_prefix=value;
                        break;
                case 1009:
                        config.pair_serial_number=value;
                        break;
		case 1010:
			if ( value > 1 ) return ILLEGAL_DATA_VALUE;
			config.world_to_xbee=value;
			break;
		case 1011:
			if ( value > 1 ) return ILLEGAL_DATA_VALUE;
			config.world_to_xport=value;
			break;
		case 1997:
			/* write default config to EEPROM */
			if ( 1 != value ) return ILLEGAL_DATA_VALUE;
			reset_cpu();
		case 1998:
			/* write default config to EEPROM */
			if ( 1 != value ) return ILLEGAL_DATA_VALUE;
			write_default_param_file();
			break;
		case 1999:
			/* write config to EEPROM */
			if ( 1 != value ) return ILLEGAL_DATA_VALUE;
			write_param_file();
			break;
		case 19999:
			/* unlock factory programming registers when we get 1802 in passcode register */
			if ( 1802 != value ) {
				timer.factory_unlocked=0;
				return ILLEGAL_DATA_VALUE;
			}
			timer.factory_unlocked=1;
			/* green LED for 2 seconds */
			timer.led_on_green=200;
			timer.led_on_red=0;
			break;
		default:
			return ILLEGAL_DATA_ADDRESS;

	}

	/* must not have triggered an exception */
	return 0;
}

void modbus_process(void) {
	int16 start_addr;
	int16 num_registers;
	exception result;
	int8 i;


	/* check for message */
	if ( modbus_kbhit() ) {
		/* check if it is addressed to us */
		if ( modbus_rx.address==config.modbus_address ) {	
			/* green LED for 200 milliseconds */
			timer.led_on_green=20;
			timer.led_on_red=0;

			switch(modbus_rx.func) {
				case FUNC_READ_HOLDING_REGISTERS: /* 3 */
				case FUNC_READ_INPUT_REGISTERS:   /* 4 */
					start_addr=make16(modbus_rx.data[0],modbus_rx.data[1]);
					num_registers=make16(modbus_rx.data[2],modbus_rx.data[3]);
	
					/* make sure our address is within range */
					if ( ! modbus_valid_read_registers(start_addr,start_addr+num_registers) ) {
					    modbus_exception_rsp(config.modbus_address,modbus_rx.func,ILLEGAL_DATA_ADDRESS);
						timer.modbus_last_error=ILLEGAL_DATA_ADDRESS;

						/* red LED for 1 second */
						timer.led_on_red=100;
						timer.led_on_green=0;
					} else {
						modbus_read_register_response(config.modbus_address,start_addr,num_registers);
					}
					break;
				case FUNC_WRITE_SINGLE_REGISTER: /* 6 */
					start_addr=make16(modbus_rx.data[0],modbus_rx.data[1]);

					/* try the write */
					result=modbus_write_register(start_addr,make16(modbus_rx.data[2],modbus_rx.data[3]));

					if ( result ) {
						/* exception */
						modbus_exception_rsp(config.modbus_address,modbus_rx.func,result);
						timer.modbus_last_error=result;

						/* red LED for 1 second */
						timer.led_on_red=100;
						timer.led_on_green=0;
					}  else {
						/* no exception, send ack */
						modbus_write_single_register_rsp(config.modbus_address,
							start_addr,
							make16(modbus_rx.data[2],modbus_rx.data[3])
						);
					}
					break;
				case FUNC_WRITE_MULTIPLE_REGISTERS: /* 16 */
					start_addr=make16(modbus_rx.data[0],modbus_rx.data[1]);
					num_registers=make16(modbus_rx.data[2],modbus_rx.data[3]);

					/* attempt to write each register. Stop if exception */
					for ( i=0 ; i<num_registers ; i++ ) {
						result=modbus_write_register(start_addr+i,make16(modbus_rx.data[5+i*2],modbus_rx.data[6+i*2]));

						if ( result ) {
							/* exception */
							modbus_exception_rsp(config.modbus_address,modbus_rx.func,result);
							timer.modbus_last_error=result;
	
							/* red LED for 1 second */
							timer.led_on_red=100;
							timer.led_on_green=0;
			
							break;
						}
					}
		
					/* we could have gotten here with an exception already send, so only send if no exception */
					if ( 0 == result ) {
						/* no exception, send ack */
						modbus_write_multiple_registers_rsp(config.modbus_address,start_addr,num_registers);
					}

					break;  
				default:
					/* we don't support most operations, so return ILLEGAL_FUNCTION exception */
					modbus_exception_rsp(config.modbus_address,modbus_rx.func,ILLEGAL_FUNCTION);
					timer.modbus_last_error=ILLEGAL_FUNCTION;

					/* red led for 1 second */
					timer.led_on_red=100;
					timer.led_on_green=0;
			}
		} else {
			/* MODBUS packet for somebody else */
			/* yellow LED 200 milliseconds */
			timer.led_on_green=20;
			timer.led_on_red=20;
		}
	}
}