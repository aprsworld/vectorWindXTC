#inline
int8 xor_crc(int8 oldcrc, int8 data) {
	return oldcrc ^ data;
}

int8 EEPROMDataRead(int16 start, int8 *data, int16 count ) {
	int8 crc=0;
	int16 address=0;

	for ( address=0 ; address<count ; address++ ) {
		restart_wdt();
		*data = read_program_eeprom( start + address*2 );
		crc = xor_crc(crc,*data);
		data++;
	}
	return crc;
}

int8 EEPROMDataWrite(int16 start, int8 *data, int16 count ) {
	int8 crc=0;
	int16 address=0;

	for ( address=0 ; address<count ; address++ ) {
		restart_wdt();
		crc = xor_crc(crc,*data);
		write_program_eeprom( start + address*2, *data );
		data++;
	}

	return crc;
}

void write_param_file() {
	int8 crc;


	/* erase 512 byte chunks of flash before write */
	erase_program_eeprom(PARAM_CRC_ADDRESS);
	erase_program_eeprom(PARAM_CRC_ADDRESS+512);


	/* write the config structure */
	crc = EEPROMDataWrite(PARAM_ADDRESS,(void *)&config,sizeof(config));
	/* write the CRC that was calculated on the structure */
	write_program_eeprom(PARAM_CRC_ADDRESS,crc);
}

void write_default_param_file() {
	int8 i;

	/* green LED for 1.5 seconds */
	timers.led_on_green=150;

	config.modbus_address=28;
	config.modbus_mode=MODBUS_MODE_RTU;

	config.serial_prefix=SERIAL_PREFIX_DEFAULT;
	config.serial_number=SERIAL_NUMBER_DEFAULT;


	config.adc_sample_ticks=20;
	
	for ( i=0 ; i<sizeof(config.adc_offset) ; i++ ) {
		config.adc_offset[i]=0;
	}

	config.seconds_telem=10;

	config.load_control_mode=LOAD_MODE_AUTOMATIC;
	config.load_insolation_on=12;
	config.load_insolation_off=24;
	config.load_volts_disconnect=235;
	config.load_volts_reconnect=249;
	config.load_transition_delay=300; /* 300 seconds default */
	config.load_evening_seconds=21600;  /* 21600 (6 hours) */
	config.load_morning_seconds=7200;   /* 7200 (2 hours) */

	/* write them so next time we use from EEPROM */
	write_param_file();

}


void read_param_file() {
	int8 crc;

	crc = EEPROMDataRead(PARAM_ADDRESS,(void *)&config, sizeof(config)); 

	if ( crc != read_program_eeprom(PARAM_CRC_ADDRESS) ) {
		write_default_param_file();
	}
}


