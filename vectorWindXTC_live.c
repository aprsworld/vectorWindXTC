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

void live_send_all(void) {
	int8 buff[68];
	int16 l,lCRC;
	int8 i;

	if ( 255==timers.xbee_wakeup )
		timers.xbee_wakeup=0;

	if ( timers.xbee_wakeup < 5 ) {
		timers.xbee_shutdown=25;
		output_low(XBEE_SLEEP);
		return;
	}


	buff[0]='#';
	buff[1]=config.serial_prefix;
	buff[2]=make8(config.serial_number,1);
	buff[3]=make8(config.serial_number,0);
	buff[4]=sizeof(buff); /* packet length */
	buff[5]=26; /* packet type */
	buff[6]=make8(current.sequence_number,1);
	buff[7]=make8(current.sequence_number,0);

	/* analog values */
	for ( i=0 ; i<sizeof(config.adc_offset) ; i++ ) {
		l=map_modbus(i*2);
		buff[i*4+8] =make8(l,1);
		buff[i*4+9] =make8(l,0);
		l=map_modbus(i*2+1);
		buff[i*4+10] =make8(l,1);
		buff[i*4+11] =make8(l,0);

	}

	/* counter values */
	for ( i=48 ; i<=65 ; i++ ) {
		buff[i]=0;
	}	

	disable_interrupts(GLOBAL);
	buff[48]=make8(current.pulse_count,1);
	buff[49]=make8(current.pulse_count,0);
	buff[50]=make8(current.pulse_period,1);
	buff[51]=make8(current.pulse_period,0);
	buff[52]=make8(current.pulse_min_period,1);
	buff[53]=make8(current.pulse_min_period,0);

	current.pulse_count=0;
	current.pulse_period=0;
	current.pulse_min_period=0xffff;
	enable_interrupts(GLOBAL);

	/* calculate CRC */
	lCRC=crc_chk(buff+1,65);
	buff[66]=make8(lCRC,1);
	buff[67]=make8(lCRC,0);

	for ( i=0 ; i<sizeof(buff) ; i++ ) {
		/* xbee modem */
		fputc(buff[i],WORLD_SERIAL);
	}	

	current.sequence_number++;
	timers.now_telem=0;
	xbee_off();

}
