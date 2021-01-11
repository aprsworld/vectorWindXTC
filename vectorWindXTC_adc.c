int16 adc_offset(int8 ch, int16 adc) {
	if ( bit_test(config.adc_offset[ch],7) ) {
		if ( adc > (config.adc_offset[ch]&0b01111111) ) 
			adc-=(config.adc_offset[ch]&0b01111111);	
	} else {
		/* don't need to check for overflow, since ADC is 0 to 2^10 and adc_offset is 2^8 ... can't overflow */
		adc+=config.adc_offset[ch];
	}
	return adc;
}

int16 adc_get_now(int8 ch) {
	return adc_offset(ch,current.adc_buffer[ch][current.adc_buffer_index]);
}

int16 adc_get_avg(int8 ch) {
	int16 sum;
	int8 i;

	// Calculate the mean.  This is done by summing up the
	// values and dividing by the number of elements.
	sum = 0;
	for( i = 0; i < 16 ; i++ ) {
		sum += current.adc_buffer[ch][i];;
	}

	/* divide sum by our 16 samples and round by adding 8 */
	sum = ( (sum+8) >> 4 );
	return adc_offset(ch,sum);
}


void adc_update(void) {
	int8 i;


	/* wrap buffer around */
	current.adc_buffer_index++;
	if ( current.adc_buffer_index >= 16 ) {
		current.adc_buffer_index=0;
	}

	for ( i=0 ; i<4 ; i++ ) {
		set_adc_channel(i);
		current.adc_buffer[i][current.adc_buffer_index] = read_adc();
	}

	/* wind generator input */
	current.adc_buffer[4][current.adc_buffer_index]=make16(ltc4151_read(0xce, 0x02),ltc4151_read(0xce, 0x03))>>4; /* voltage */
	current.adc_buffer[5][current.adc_buffer_index]=make16(ltc4151_read(0xce, 0x00),ltc4151_read(0xce, 0x01))>>4; /* current */

	/* solar input */
	current.adc_buffer[6][current.adc_buffer_index]=make16(ltc4151_read(0xd0, 0x02),ltc4151_read(0xd0, 0x03))>>4; /* voltage */
	current.adc_buffer[7][current.adc_buffer_index]=make16(ltc4151_read(0xd0, 0x00),ltc4151_read(0xd0, 0x01))>>4; /* current */

	/* light load output */
	current.adc_buffer[8][current.adc_buffer_index]=make16(ltc4151_read(0xd2, 0x02),ltc4151_read(0xd2, 0x03))>>4; /* voltage */
	current.adc_buffer[9][current.adc_buffer_index]=make16(ltc4151_read(0xd2, 0x00),ltc4151_read(0xd2, 0x01))>>4; /* current */


}
