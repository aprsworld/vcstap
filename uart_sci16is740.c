/* register map for SCI16IS740 */
#define UART_RHR          0x00 //  Recv Holding Register is 0x00 in READ Mode
#define UART_THR          0x00 //  Xmit Holding Register is 0x00 in WRITE Mode
#define UART_IER          0x01  // Interrupt Enable Register
#define UART_FCR          0x02  // FIFO Control Register in WRITE Mode
#define UART_LCR          0x03  // Line Control Register
#define UART_MCR          0x04  // Modem Control Register
#define UART_LSR          0x05  // Line status Register
#define UART_MSR          0x06  // Modem Status Register
#define UART_SPR          0x07  // ScratchPad Register
#define UART_TCR          0x06  // Transmission Control Register
#define UART_TLR          0x07  // Trigger Level Register
#define UART_TXLVL        0x08  // Xmit FIFO Level Register
#define UART_RXLVL        0x09  // Recv FIFO Level Register
#define UART_EFCR         0x0F  // Extra Features Control Register

#define UART_DLL          0x00  // Divisor Latch LSB  0x00
#define UART_DLH          0x01  // Divisor Latch MSB  0x01

#define UART_EFR          0x02  // Enhanced Function Register

#define UART_I2C_WRITE    0x00
#define UART_I2C_READ     0x01                                               

/* A0 and A1 at VSS */
#define UART_ADDR         0x9A

int8 uart_read(int8 regaddr) {
	int8 data;

	i2c_start();
	delay_us(15);
	i2c_write(UART_ADDR);
	i2c_write(regaddr<<3);
	i2c_start();
	delay_us(15);
	i2c_write(UART_ADDR | UART_I2C_READ);  // read cycle                                 
	data=i2c_read(0);
	i2c_stop();

	return data;
}

void uart_write(int8 regaddr, int8 data ) {                                                                  
	i2c_start();
	delay_us(15);                                                 
	i2c_write(UART_ADDR); // write cycle                       
	i2c_write(regaddr<< 3);  // write cycle         
	i2c_write(data);
	i2c_stop();
} 

void uart_putc(int8 data ) {
	uart_write(UART_THR, data);  // send data to UART Transmit Holding Register
}

int1 uart_kbhit(void) {
	return (uart_read(UART_LSR) & 0x01);
}

#inline
int8 uart_getc() {
	return uart_read(UART_RHR);
}

void uart_init(void) {
	output_low(UART_RESET);
	delay_ms(10);
	output_high(UART_RESET);
	delay_ms(10);

	/* UART divisor calculator spreadsheet uart_divisor_calc.xls */
	uart_write(UART_LCR, 0x80); // 0x80 to program baud rate divisor
	uart_write(UART_DLL, 12);    // divide clock by 12 for 9600 baud when using 1.8432 crystal
	uart_write(UART_DLH, 0);

	uart_write(UART_LCR, 0xBF); // access EFR register
	uart_write(UART_EFR, 0x10); // enable enhanced registers
 	uart_write(UART_LCR, 0x03); // 8 data bits, 1 stop bit, no parity
//	uart_write(UART_IER, 0x01); // enable interrupt on receive data becomming available
	uart_write(UART_FCR, 0x07); // reset TXFIFO, reset RXFIFO, enable FIFO mode
}

