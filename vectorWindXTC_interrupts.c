

#int_timer2  HIGH
/* this timer only runs once booted */
void isr_100us(void) {
	static int8 tick=0;

	/* anemometer polling state variables */
	/* anemometer 0 / PIN_B0 */
	short ext0_count;
	short ext0_now;
	static short ext0_last=0;
	static short ext0_state=0;


	/* every 100 cycles we tell main() loop to do 10 milisecond activities */
	tick++;
	if ( 100 == tick ) {
		tick=0;
		action.now_10millisecond=1;
	}
	

	/* count time between falling edges */
	if ( ext0_count && 0xffff != timers.pulse_period )
		timers.pulse_period++;

	/* anemometer 0 / PIN_B0 trigger on falling edge */
	ext0_now=input(PIN_B0);
	if ( 0 == ext0_now && 1 == ext0_last ) {
		current.pulse_count++;
		if ( 1 == ext0_state ) {
			/* currently counting, time to finish */
			ext0_count=0;
			current.pulse_period=timers.pulse_period;
			if ( current.pulse_period < current.pulse_min_period ) {
				current.pulse_min_period=current.pulse_period;
			}
			ext0_state=0;
		}
		if ( 0 == ext0_state ) {
			/* not counting, time to start */
			timers.pulse_period=0;
			ext0_count=1;
			ext0_state=1;
		}
	}
	ext0_last = ext0_now;

	if ( action.now_strobe_counters ) {
		action.now_strobe_counters=0;

		/* save (strobe) our data */
		current.strobed_pulse_period=current.pulse_period;
		current.strobed_pulse_min_period=current.pulse_min_period;
		current.strobed_pulse_count=current.pulse_count;

		/* reset our data */
		current.pulse_period=0;
		current.pulse_min_period=65535;
		current.pulse_count=0;

		/* counters are copied, now we can start the rest of the measurements */
		action.now_gnss_trigger_start=1;
	}
}




#int_rda2
void serial_isr_wireless(void) {
	int8 c;

	c=fgetc(SERIAL_XTC);
}

#define GNSS_STATE_WAITING 0
#define GNSS_STATE_IN      1

#int_rda
void serial_isr_gnss(void) {
	static int1 state=GNSS_STATE_WAITING; 
	static int8 buff[6];
	static int8 pos;
	int8 c;

	c=fgetc(SERIAL_GNSS);


	/* skip adding if ( carriage returns OR line feeds OR buffer full ) */
	if ( '\r' != c && '\n' != c && pos < (sizeof(nmea_raw.buff)-2) ) {
		/* add to our buffer */
		nmea_raw.buff[pos]=c;
		pos++;
		nmea_raw.buff[pos]='\0';
	}

	/* capturing data but looking for our trigger so we can set flag */
	if ( GNSS_STATE_WAITING == state ) {
		/* FIFO */
		buff[0]=buff[1];            // '$'
		buff[1]=buff[2];            // 'G'
		buff[2]=buff[3];            // 'P'
		buff[3]=buff[4];            // 'R' or 'H'
		buff[4]=buff[5];            // 'M' or 'D'
		buff[5]=c; 					// 'C' or 'T'
	
		if ( '$' == buff[0] && NMEA0183_TRIGGER[0] == buff[1] && 
	 	                       NMEA0183_TRIGGER[1] == buff[2] &&
		                       NMEA0183_TRIGGER[2] == buff[3] &&
		                       NMEA0183_TRIGGER[3] == buff[4] &&
	 	                       NMEA0183_TRIGGER[4] == buff[5] ) {
			/* matched our 5 character trigger sequence */
			action.now_strobe_counters=1;
			current.gnss_age=0;
			state=GNSS_STATE_IN;

//			output_toggle(LED_RED);
		}
	} else {
		/* GNSS_STATE_IN */
		/* in our trigger sentence. Look for '\r' or '\n' that ends it */
		if ( '\r' == c || '\n' == c ) {
			for ( c=0 ; c<pos ; c++ ) {
				current.gnss_buff[c]=nmea_raw.buff[c];
			}
			current.gnss_buff[c]='\0';
			current.gnss_length=pos;
			
			action.now_gnss_trigger_done=1;
			state=GNSS_STATE_WAITING;
			pos=0;
		}
	}

}
